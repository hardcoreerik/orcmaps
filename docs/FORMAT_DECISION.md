# Map pack format decision

Status: **decided** for the container format (PMTiles v3), **deferred** for
tile content (raster-cached-from-vector vs. pure vector) pending the Lane
County vertical slice's measured render times. This document records the
evidence, not just the conclusion, per project policy of not choosing a
format for convenience.

## Candidates evaluated

### 1. PMTiles v3

Spec: `github.com/protomaps/PMTiles` (`spec/v3/spec.md`).

- Single flat, seekable archive file. Fixed 127-byte header at offset 0
  (magic `"PMTiles"`, version, offsets/lengths for root directory,
  metadata, leaf directories, tile data; tile count fields; compression
  codes; tile type; zoom range; bounding box).
- **Root directory is capped at ≤16,257 bytes compressed** by the spec —
  deliberately sized to be one small, bounded read. If an archive's full
  directory doesn't fit, root entries with `RunLength=0` point to **leaf
  directories** stored later in the file.
- Directory entries are delta-encoded, varint-packed **columnar** arrays
  (TileID delta, RunLength, Length, Offset — not fixed-size structs), then
  the whole buffer is compressed (gzip by default).
- Tile addressing uses a **Hilbert curve** tile ID across a stacked pyramid
  of zoom levels (z0=ID 0; z1=IDs 1-4; etc.), which preserves spatial
  locality — long runs of identical/sparse tiles (e.g. open ocean) collapse
  into one directory entry with a large `RunLength`. The v3 design blog
  (`protomaps.com/blog/pmtiles-v3-layout-compression`) shows a worked
  planet-scale example: one ocean entry with `RunLength=107,977` replacing
  107,977 separate entries (1.8 MB → 24 bytes of directory).
- **Lookup cost is bounded**: read+decompress root dir (once, cached) →
  binary search → at most one leaf-directory read+decompress → binary
  search → one tile-data byte-range read. Worst case 3 seeks, no SQL engine,
  no write path, no locking semantics to reason about.
- **Real embedded precedent, not just spec compliance on paper**:
  `yuiseki/m5-cardputer-offgrid-tiny-map` (Apache-2.0) renders a **~78 GiB,
  z0-14, whole-planet vector PMTiles archive** on an ESP32-S3 Cardputer,
  split into 2 GiB chunks purely to work around FAT32's 4 GiB single-file
  limit. Its downstream fork `yuiseki/m5-cardputer-meshtastic-map` documents
  concrete numbers: runs **without PSRAM**, ~95 KB free heap with the map
  component loaded, needs one 32 KB contiguous scratch block, z13 tile
  render 33s→8.1s after optimization, z0 render 0.4s. This is direct
  evidence the format (not just a raster-tile cache, but real streaming
  vector-tile decode) is viable on hardware weaker than our ESP32-P4 Tab5
  target (which has PSRAM and a much larger heap).
- Tooling ecosystem: `pmtiles` CLI/Python/Go/JS libraries, Planetiler,
  tippecanoe, MapLibre — an archive we build is inspectable/verifiable with
  off-the-shelf tools, not just our own code. We used the official Python
  `pmtiles` package (BSD-3-Clause) as a build tool to generate our own test
  fixture (see `tests/fixtures/generate_fixture.py`) rather than hand-
  rolling the binary format — cross-validated against the reference
  implementation from day one.

### 2. MBTiles

Spec: `github.com/mapbox/mbtiles-spec`. SQLite database (`tiles` table:
`zoom_level, tile_column, tile_row, tile_data`; TMS y-flipped row numbering,
`tile_row = 2^zoom - 1 - y`).

Rejected as the container format for the embedded reader:

- Requires an embedded SQLite engine. SQLite's correctness guarantees assume
  proper file-locking and, for WAL mode, a shared-memory `-shm` companion
  file — semantics that don't map cleanly onto FAT32-over-SD-over-SPI.
  Community reports of ESP32 SQLite ports describe locking as effectively
  absent (safe for read-only single-reader use, not more), with inconsistent
  SD I/O performance reported in forum threads.
- Even in the best case (single reader, no locking needed), it's a strictly
  heavier dependency than a flat file + binary search: full SQL engine,
  B-tree page cache, its own I/O assumptions — for a benefit (ad hoc SQL
  querying) OrcMaps doesn't need, since it only ever does one query shape
  (z/x/y → blob).
- Not rejected as *impossible* — a careful read-only, single-connection,
  no-WAL configuration could probably work — but PMTiles gets the same
  z/x/y→bytes lookup with less machinery and has the concrete embedded
  precedent MBTiles-on-ESP32 lacks. Documenting this so a future contributor
  doesn't re-litigate it without new evidence.

### 3. Custom `ORCMAP2` format

Rejected for the *container* layer: PMTiles already gives bounded-memory
seek+read, a public spec, existing tooling, and a working ESP32-S3
precedent. Inventing our own container would mean re-deriving all of that
(directory compaction, spatial locality, bounded root-directory size) with
no measured advantage, which violates the project's own rule against a
proprietary format chosen "merely because it's easier initially."

**Where a custom, OrcMaps-specific *encoding* still may be justified**: the
tile *content* schema — i.e., whether tiles hold general-purpose vector
data (MVT/protobuf, arbitrary layers/attributes per the OpenMapTiles or
Shortbread schema) or a much narrower, OrcMaps-specific pre-simplified
feature set (only the categories ORCMAP1 already draws: road/water/airport/
label, quantized to tile-local coordinates, no general protobuf parsing).
This is **not yet decided** — see "Deferred decision" below. The container
being PMTiles does not force the payload schema to be general OpenMapTiles-
style MVT; PMTiles' `tile_type` byte only says "this archive holds MVT" as
one of several recognized values, but the *tiler* we build (or reuse) is a
separate decision from the archive format.

### Raster vs. vector vs. hybrid

- **Pure raster** (pre-rendered PNG/WebP tiles per zoom): simplest decoder
  (image decode only, no geometry/label logic on-device), but a full
  z0-14 world raster set is dramatically larger than vector for the same
  visual density, and re-styling (dark mode, high-contrast) requires
  rebuilding every tile rather than changing a style table.
- **Pure vector, decoded live every frame**: smallest storage footprint,
  full style flexibility, but repeated per-frame protobuf decode + line/
  label drawing is the most CPU-expensive option — acceptable for a static
  view, questionable for smooth panning without caching.
- **Vector tiles, decoded once, rendered to an RGB565 raster and cached**
  (RAM and/or SD): this is what the yuiseki precedent actually does, and
  what ORCMAP1's own `M5Canvas` sprite cache in `adsb_dashboard.cpp`
  already does informally at the *viewport* level (cache the whole rendered
  scene, not per-tile). This hybrid gets vector's storage/style advantages
  with raster's cheap repeated-draw cost, at the price of a slower first
  view of any given area. The yuiseki numbers (z13: 33s→8.1s optimized,
  z0: 0.4s) are exactly this first-render cost.

**Decision: vector tiles in PMTiles, decoded once per tile and cached as
RGB565 (RAM/PSRAM first, optional SD-backed cache second)** — following the
proven pattern, not inventing a new one. Raster-only is rejected primarily
on the world-scale storage/style-rebuild argument; live-per-frame vector
decode is rejected as strictly worse than tile-level caching for the same
data with no upside.

## Decided: container format

**PMTiles v3.** Reasons, in order of weight:

1. Concrete embedded precedent at a harder memory budget than our target
   (ESP32-S3, no PSRAM, ~95 KB heap vs. our ESP32-P4 Tab5 with PSRAM).
2. Bounded, bounded-again lookup cost (root dir ≤16 KB, ≤1 leaf dir, 1 tile
   read) with no SQL engine and no locking-semantics risk on FAT32/SD.
3. Public spec + existing tooling means packs are inspectable/debuggable
   outside our own code, and a real "extract Oregon from a planet file"
   workflow already exists (`pmtiles extract`, Planetiler) rather than us
   building that from scratch.
4. Standard `.pmtiles` extension and format, per explicit project direction
   — packs OrcMaps produces are usable by any PMTiles-aware tool, and any
   properly-licensed `.pmtiles` archive from the wider ecosystem is a
   candidate OrcMaps input, not just our own build pipeline's output.

## Deferred: tile content schema

Not yet decided whether the pack builder emits general OpenMapTiles/
Shortbread-schema MVT tiles (reuse existing tilers like Planetiler
unmodified — less pipeline work, but the on-device MVT decoder must
handle a real-world schema's layer/attribute variety) or a narrower,
OrcMaps-defined tile schema carrying only the feature categories ORCMAP1
already draws (less on-device parsing complexity and a much smaller/
simpler decoder, at the cost of a custom Planetiler/tippecanoe config or a
purpose-built tiler).

This is deferred to the Lane County vertical slice: build one real Lane
County `.pmtiles` archive with a standard schema (Planetiler + OpenMapTiles
profile) first, measure actual on-device MVT decode time/complexity against
ORCMAP1's baseline, and only invest in a narrower custom schema if that
measurement shows a real, not hypothetical, cost worth avoiding. This
follows the project's own instruction not to reject a standard approach
without measurements, and not to build a narrower format "because it's
easier" without evidence.

## What this does NOT decide

- Whether world/country/state/local packs share one schema (they should —
  the whole point is one rendering path for all pack sizes) is assumed but
  not yet validated against a built country-or-larger pack.
- Exact zoom range per tier — `docs/PACK_FORMAT.md` will record this once
  the Lane County pack's actual size-per-zoom is measured (published planet-
  scale numbers found in research — ~120 GB at z0-15 per Protomaps'
  own basemap docs, ~78 GiB at z0-14 for the yuiseki OpenMapTiles-schema
  build — are not precise enough to derive Oregon- or Lane-County-scale
  numbers from; those need to come from actually building the pack).
