# OrcMaps Architecture

How the system actually works, as of 2026-09-13. Where a component is
described but not yet implemented, this document says so explicitly rather
than describing aspirational behavior as real — see `PROJECT_TRUTH.md`
"Document authority" and `STATUS.md` for the authoritative implementation-
state summary.

## System overview

```mermaid
flowchart TD
    Pack["Map Pack (.pmtiles)"]
    Source["orcmap::ByteSource"]
    Reader["orcmap::PmTilesReader"]
    Tile["Tile Decoder (MVT) -- NOT IMPLEMENTED"]
    Style["orcmap::MapStyle / ResolveFeatureStyle"]
    Cache["Rendered Tile Cache -- NOT IMPLEMENTED"]
    Renderer["Renderer (M5GFX adapter) -- NOT IMPLEMENTED"]
    Overlay["Overlay Layer -- NOT IMPLEMENTED"]
    Display["Display"]

    Pack --> Source
    Source --> Reader
    Reader --> Tile
    Tile --> Renderer
    Style --> Renderer
    Cache <--> Renderer
    Renderer --> Display
    Overlay --> Renderer
```

Implemented today: `Pack -> Source -> Reader` (fully, host-tested) and
`Style` (fully, host-tested, but not yet wired to any renderer since none
exists). Everything marked "NOT IMPLEMENTED" is real, scoped, and described
below so the next work session builds toward this shape rather than
guessing at it — see `ROADMAP.md` for sequencing.

## Major components

| Component | Path | Status |
|---|---|---|
| `orcmap::ByteSource` | `include/orcmap/byte_source.hpp` | Implemented |
| `orcmap::host::FileByteSource` | `adapters/host/file_byte_source.{hpp,cpp}` | Implemented (host only) |
| ESP-IDF `ByteSource` adapter | `adapters/esp_idf/` | Not implemented (empty dir) |
| Geo/tile math | `include/orcmap/geo.hpp`, `src/core/geo.cpp` | Implemented |
| `orcmap::PmTilesReader` | `include/orcmap/pmtiles.hpp`, `src/tiles/pmtiles_reader.cpp` | Implemented (container layer only) |
| Vector tile (MVT) decoder | `include/orcmap/mvt.hpp`, `src/tiles/mvt_decoder.cpp` | Implemented (schema-agnostic container decode only; no FeatureKind mapping) |
| `orcmap::MapStyle` / style system | `include/orcmap/style.hpp`, `include/orcmap/color.hpp`, `include/orcmap/cache_key.hpp`, `src/render/style.cpp` | Implemented |
| Renderer core | `src/render/` | Not implemented (only style.cpp exists in this dir so far) |
| M5GFX adapter | `adapters/m5gfx/` | Not implemented (empty dir) |
| Overlay primitives | `src/overlays/` | Not implemented (empty dir) |
| Tile/rendered-tile cache | `src/cache/` | Not implemented (empty dir); `RenderedTileCacheKey` shape exists in `include/orcmap/cache_key.hpp` |
| Pack builder | `tools/pack-builder/` | Not implemented (empty dir) |
| Pack inspector/verifier | `tools/pack-inspect/`, `tools/pack-verify/` | Not implemented (empty dirs) |
| Examples | `examples/m5stack-tab5/`, `examples/generic-esp32/` | Not implemented (empty dirs) |
| Host tests | `tests/host/` | Implemented, 100% passing |
| Test fixture | `tests/fixtures/tiny.pmtiles` (+ `generate_fixture.py`) | Implemented |
| Runtime attribution API | `include/orcmap/attribution.hpp`, `include/orcmap/map_source.hpp` | Implemented (header-only; no `MapEngine`/discovery populates it yet) |
| Consumer build gate | `tests/consumer/` | Implemented, passing |
| Data provenance registry | `data/sources/*.json` | Implemented, 9 records (2 `CONFIRMED`, rest `REVIEW_REQUIRED` pending primary-source verification or per-record filtering -- see `docs/DATA_PROVENANCE_REGISTRY.md`) |
| Pack manifest schema | `docs/PACK_MANIFEST_SCHEMA.md` | Documented, not implemented (no pack builder exists) |

## Repository layout

```
include/orcmap/      Public headers: byte_source.hpp, geo.hpp, pmtiles.hpp,
                      color.hpp, style.hpp, cache_key.hpp, attribution.hpp,
                      map_source.hpp, mvt.hpp
src/core/             geo.cpp (Web Mercator tile math)
src/tiles/            pmtiles_reader.cpp (PMTiles v3 container reader),
                      mvt_decoder.cpp (schema-agnostic MVT geometry decoder)
src/render/           style.cpp (built-in styles + resolver). Renderer
                      core itself not yet added here.
src/storage/          Empty -- reserved for any storage-layer logic beyond
                      the ByteSource interface itself (interface lives in
                      include/, not here).
src/overlays/         Empty -- marker/polyline/polygon primitives, planned.
src/cache/            Empty -- bounded LRU tile cache, planned.
adapters/host/        file_byte_source.{hpp,cpp} -- stdio ByteSource for
                      host tests and future host-side tools.
adapters/m5gfx/       Empty -- M5GFX/LovyanGFX rendering backend, planned.
adapters/esp_idf/     Empty -- ESP-IDF filesystem ByteSource, planned.
tools/pack-builder/   Empty -- OSM extract -> .pmtiles, planned.
tools/pack-inspect/   Empty -- inspect a pack's metadata/contents, planned.
tools/pack-verify/    Empty -- validate a pack against its manifest, planned.
examples/             Empty -- Tab5 and generic ESP32-S3 examples, planned.
tests/host/           Host-buildable unit tests (no ESP-IDF needed).
tests/consumer/       External-consumer build gate -- public headers only,
                      see "Consumer integration test" below.
tests/fixtures/       Committed test fixtures + the script that builds them.
third_party/miniz/    Vendored miniz (tinfl inflate subset), MIT license.
data/sources/         Data provenance registry: one JSON record per
                      reviewed map-data source, see "Data provenance and
                      attribution" below.
docs/                 Architecture/decision/porting/licensing documents.
```

## Data flow (as implemented)

1. A caller constructs a `ByteSource` over a `.pmtiles` file (today:
   `orcmap::host::FileByteSource` on the host; on-device this will be an
   `adapters/esp_idf` implementation wrapping OrcSDR's
   `orcsdr::storage::FileSystem` or raw ESP-IDF VFS — not yet written).
2. `orcmap::PmTilesReader reader(&source); reader.Open();` parses the
   127-byte header and reads+decompresses the root directory (≤16,257
   bytes compressed per spec) into `root_dir_`. Nothing else is loaded.
3. `reader.GetTile(z, x, y, &out)` computes the tile's Hilbert `tile_id`
   (`orcmap::ZxyToTileId`), binary-searches the root directory, follows at
   most one leaf-directory indirection if the archive's directory didn't
   fit in the root alone, and reads the tile's bytes directly from the
   archive at the resolved offset/length. Returns `false` (not an error)
   for a sparse/missing tile.
4. **Not yet implemented:** decoding those bytes as MVT vector geometry,
   resolving each decoded feature's `orcmap::FeatureKind` and calling
   `ResolveFeatureStyle()` for its paint, drawing through a renderer
   backend, and caching the rendered result.

## Map pack / archive layer

`docs/PACK_FORMAT.md` is the pack-specific reference; the byte format
itself is specified by PMTiles v3
(`github.com/protomaps/PMTiles/blob/main/spec/v3/spec.md`), not re-
described here. `PmTilesReader` implements: 127-byte header parse, unsigned
LEB128 varint decoding, delta-decoded/columnar directory entry parsing
(tile_id delta array, run_length array, length array, offset array with
the spec's "0 means contiguous with previous entry" convention), gzip
inflate via vendored miniz (`Inflate()`/`StripGzipWrapper()` in
`pmtiles_reader.cpp` — PMTiles uses full RFC 1952 gzip framing around the
directory, not bare zlib/deflate, so the gzip header/trailer are stripped
before handing the raw deflate stream to `tinfl_decompress_mem_to_heap`),
and the standard integer Hilbert curve `xy2d` algorithm stacked per zoom
level (`HilbertXyToIndex`/`TilesBeforeLevel`/`ZxyToTileId`).

Corruption handling implemented and tested: bad magic, unsupported version,
oversized root directory (>16,384 bytes), truncated reads, and a bounded
leaf-directory hop count (8) to reject cyclic/corrupt archives — all
covered by `tests/host/test_pmtiles.cpp`.

## Byte source / storage layer

See `PROJECT_TRUTH.md` "Storage Model". Three methods, no more:
`Read(offset, dest, length) -> size_t`, `Size() -> uint64_t`,
`Valid() -> bool`. `PmTilesReader` holds a raw, non-owning `ByteSource*` —
the caller controls the source's lifetime, matching
`orcsdr_storage::File`'s existing seek+read shape closely enough that an
ESP-IDF adapter should be a thin wrapper, not a rewrite (see
`docs/ORCMAP1_AUDIT.md` §7).

## Tile lookup

Bounded by construction: at most one root-directory binary search, at most
one leaf-directory fetch+decompress+binary-search (spec allows nesting;
`GetTile()` bounds this to 8 hops to reject corrupt/cyclic input rather
than looping), and one tile-data byte-range read. No full-archive scan, no
full-archive load, regardless of archive size — this is what makes a
50-100+ GB world archive architecturally the same code path as the 519-byte
test fixture.

## Vector tile decode

Implemented: `orcmap::DecodeMvtTile()` (`include/orcmap/mvt.hpp`,
`src/tiles/mvt_decoder.cpp`) decodes MVT-encoded tile bytes (as returned
by `PmTilesReader::GetTile()`, already decompressed by the caller) into
`MvtTile` -> `MvtLayer[]` -> `MvtFeature[]`, each feature carrying its
geometry (as rings of tile-local `MvtPoint{x,y}`, one ring per
`MoveTo`/polygon-ring), geometry type (point/linestring/polygon), and
attributes (parallel `attribute_keys`/`attribute_values` arrays, values as
an `std::variant<monostate, string, double, int64_t, uint64_t, bool>`).

Implemented as a minimal, hand-rolled protobuf-wire-format reader scoped
to exactly the four MVT message shapes (Tile/Layer/Feature/Value) — not a
general-purpose protobuf parser, and not a dependency on any existing MVT
library. See `docs/DEPENDENCY_LEDGER.md` "Resolved: MVT decoding" for why.
Geometry command decoding (MoveTo/LineTo/ClosePath, zigzag-delta-encoded
coordinates) follows the public MVT spec directly.

**Deliberately schema-agnostic**: this decoder has no opinion about what a
layer named `"road"` or an attribute named `"class"` means — it decodes
exactly what's encoded, nothing more. Mapping decoded features to
`orcmap::FeatureKind` (`include/orcmap/style.hpp`) for rendering is a
separate, still-unbuilt step — see `docs/FORMAT_DECISION.md` "Deferred:
tile content schema", which this work does not resolve, by design.

Host-tested (`tests/host/test_mvt.cpp`) against `tests/fixtures/tiny.mvt`,
a fixture built with the widely-used `mapbox-vector-tile` reference
encoder (see `tests/fixtures/generate_mvt_fixture.py`), covering all three
geometry types, all four attribute value representations actually
exercised (string/double/int64/bool), and corruption handling (empty
buffer, garbage bytes, truncated tile).

Not yet built: the renderer step that walks a decoded `MvtTile` and issues
draw calls — see "Renderer" below.

## Projection / coordinates

`include/orcmap/geo.hpp` / `src/core/geo.cpp`. Standard Web Mercator tile
math: `LatLonToTileCoord`/`LatLonToTile`/`TileToLatLon`/`TilesPerAxis`,
with `WrapLongitudeDeg` (antimeridian) and `ClampLatitudeDeg` (Mercator's
±85.05112878° limit) applied internally so the functions are always
well-defined for any finite input — no NaN/Inf, no unsigned-integer
underflow at the poles or antimeridian (a real bug of this kind was caught
by `tests/host/test_geo.cpp` and fixed during this work: clamping must
happen in floating point *before* casting to `uint32_t`, not after). RF
distance/bearing/geodesic math is explicitly out of scope here — that
stays in OrcSDR.

## Renderer

Not implemented. Per `docs/STYLING.md`, the intended contract is: renderer
code calls `orcmap::ResolveFeatureStyle(kind, zoom, style)` and draws using
the returned `orcmap::MapPaint`, never reading style colors directly or
hardcoding literals. `orcmap::Color` is plain RGBA8 with zero display-API
dependency; converting to a display's native pixel format is an adapter's
responsibility.

## Style system

Fully implemented — see `docs/STYLING.md` for the complete design (this is
the canonical reference; not duplicated here). Summary: `MapStyle` +
`FeatureRule` (15 categories × color/width/zoom-visibility) +
`ResolveFeatureStyle()` + `StyleManager` (runtime switching) +
`RenderedTileCacheKey` (style-aware cache invalidation). Four built-in
styles in `src/render/style.cpp`. Host-tested in
`tests/host/test_style.cpp`.

## Data provenance and attribution

Two related but distinct pieces, both implemented today:

- **Build-time / review-time provenance**: `data/sources/*.json`, one
  record per reviewed map-data source (`docs/DATA_PROVENANCE_REGISTRY.md`
  for the schema, `docs/DATA_AND_LICENSING.md` for the policy). Validated
  by `tools/check_data_provenance.py`
  (`.github/workflows/data-provenance.yml`), the sibling to Documentation
  Truth described below. This has nothing to do with the C++ engine at
  runtime — it's what gates which datasets are even allowed into an
  official pack build.
- **Runtime attribution**: `orcmap::AttributionInfo`
  (`include/orcmap/attribution.hpp`) and `orcmap::MapSourceInfo` +
  `CollectRequiredAttribution()` (`include/orcmap/map_source.hpp`),
  host-tested in `tests/host/test_attribution.cpp`. This is what a future
  consuming application asks the *running* engine, once pack manifests
  and discovery exist to populate `MapSourceInfo` for real — the intent
  (`docs/DATA_AND_LICENSING.md` "Runtime attribution") is that an
  application never hard-codes `"(c) OpenStreetMap contributors"`; it
  calls something like `map.activeSources()` and reads each source's
  `AttributionInfo` instead. Today these two structs exist and are
  correct, but nothing populates a real `MapSourceInfo` from an actual
  opened pack yet — that wiring depends on the pack manifest
  (`docs/PACK_MANIFEST_SCHEMA.md`) and pack discovery, neither of which
  exist yet (`ROADMAP.md`).

The core engine intentionally never renders attribution text itself
(`AttributionInfo` carries `text`/`url`, not a draw call) — this mirrors
the same reasoning as the style system not rendering itself: it would
constrain the application/renderer adapter's UI flexibility. See
`docs/STYLING.md` "Renderer relationship" for the parallel case.

## Label system

Not implemented. ORCMAP1 drew labels with no collision avoidance or
priority (`docs/ORCMAP1_AUDIT.md` §4); `MapStyle::label_scale` and
`label_priority_threshold` exist as forward-looking fields in the style
struct but nothing consumes them yet, since there is no label decode/draw
path yet.

## Overlay system

Not implemented (`src/overlays/` is empty). Intended primitives: marker,
icon, text label, polyline, polygon, circle, route, track, waypoint — all
generic, all ignorant of what an application's data means. See
`PROJECT_TRUTH.md` "Map Data vs Rendering vs Overlay Separation".

## Cache architecture

Not implemented (`src/cache/` is empty). The cache **key** shape exists and
is tested (`orcmap::RenderedTileCacheKey`, `include/orcmap/cache_key.hpp`):
`pack_id_hash, z, x, y, style_id_hash, style_version, renderer_version`.
Intended two-level design per `docs/ORCMAP1_AUDIT.md`/original design
brief: a small bounded RAM/PSRAM LRU cache of currently-needed tiles, plus
an optional SD-backed cache of expensive vector-decoded renders (RGB565) —
deletable without damaging the underlying pack.

## SD storage

Not implemented on-device. See "Byte source / storage layer" above and
`docs/ORCMAP1_AUDIT.md` §7 for the existing OrcSDR storage layer this will
adapt to.

## Background work / scheduling

Not implemented. Constraint carried forward from ORCMAP1
(`docs/ORCMAP1_AUDIT.md` §2): any pack I/O must never run on a real-time
thread (SDR/USB/audio). OrcSDR's `catalog_sync` already runs its I/O on a
dedicated pinned FreeRTOS task — the intended pattern to follow, not
reinvent, once pack download/install is built for OrcMaps-scale packs.

## OrcSDR integration boundary

Not yet wired up — OrcSDR's branch still uses its own `offline_map.cpp`.
The intended boundary, once built: OrcSDR links OrcMaps as a version-pinned
ESP-IDF component (`idf_component.yml`, already present in this repo,
`version: "0.1.0"`), never includes OrcMaps' `src/` internals directly, and
implements its own `adapters/esp_idf`-shaped `ByteSource` and any
OrcSDR-specific overlay types entirely in OrcSDR's own code.

The "never includes `src/` internals" rule is not just a convention —
`tests/consumer/` (see "Consumer integration test" below) structurally
proves a consumer can build against `include/orcmap/` + `adapters/host/`
alone, the same shape OrcSDR would use with `adapters/esp_idf/` once it
exists. Pinning follows OrcSDR's existing `esp-rtl-sdr` dependency exactly
(`git:` URL + full 40-character commit SHA, not a branch or floating tag)
— see `PROJECT_TRUTH.md` "Public API and Versioning" for the actual
manifest snippet this is modeled on.

## Error handling

`PmTilesReader::Open()`/`GetTile()`/`ReadMetadata()` never throw; they
return `bool` and leave output parameters untouched or empty on failure.
Corrupt/truncated/hostile input (bad magic, oversized directory length,
cyclic leaf-directory chains) is rejected rather than causing undefined
behavior — see `tests/host/test_pmtiles.cpp`'s `TestCorruptArchive` and the
bounded-allocation guards (`kMaxRootDirBytes`, `kMaxDirectoryReadBytes`,
the 8-hop leaf-directory limit) in `pmtiles_reader.cpp`.

## Testing architecture

All current tests build and run on a plain host C++17 toolchain with no
ESP-IDF, no hardware, via `tests/host/CMakeLists.txt` (a standalone CMake
project, not a subdirectory of the ESP-IDF component build — see "Build
system" below). No test framework dependency: a ~40-line macro-based
harness (`tests/host/test_util.hpp`) is used instead, since the test
surface doesn't yet justify one. `tests/fixtures/tiny.pmtiles` (519 bytes)
is a synthetic PMTiles archive built by the *official* Protomaps Python
`pmtiles` library (BSD-3-Clause, dev-tool only — see
`tests/fixtures/generate_fixture.py`), so the reader is validated against
an independent reference implementation, not just against itself.

Current coverage: geo/tile math (wrap, clamp, known tiles, antimeridian,
high-latitude clamping, round-trip containment), PMTiles container
(header fields, metadata, known-tile byte-exact content, sparse/missing
tiles, corrupt-file handling, Hilbert ID uniqueness/monotonicity), the
style system (built-in ids, default style, runtime switching with graceful
fallback, zoom visibility, Night style's no-blue-light constraint, style
distinctness, cache-key behavior), the runtime attribution helpers
(no-sources/excluded/included/mixed/deduplicated cases), and MVT decode
(all three geometry types, all four exercised attribute value
representations, corrupt/truncated/empty-buffer handling). All passing as
of this writing — see `STATUS.md` for how to reproduce.

## Consumer integration test

`tests/consumer/` (`CMakeLists.txt` + `consumer_smoke_test.cpp`) is a
second, separate standalone CMake project — not a subdirectory of
`tests/host/` — that builds a small executable using *only* headers under
`include/orcmap/` and `adapters/host/`, linked against the engine sources
compiled the normal way. Its own include path never adds `src/` or
`third_party/`, so a consumer-side `#include` reaching into engine
internals is a build failure, not a lint warning — see "OrcSDR integration
boundary" above and `PROJECT_TRUTH.md` "Public API and Versioning". It
exercises what's real today: tile-coordinate math, opening the fixture
archive and reading a tile, resolving a built-in style, and the runtime
attribution API. Extend it as real API surface is added; do not let it
silently stop reflecting what a real consumer would need.

## Documentation and provenance tooling

Two deterministic, stdlib-only checkers, both adapted from (or, for the
second, modeled directly on) OrcSDR's own `documentation-truth.yml`/
`check_documentation_truth.py` pattern — see `PROJECT_TRUTH.md` "Local
Development Conventions" for why both exist as project rules, not optional
tooling:

- **`tools/check_documentation_truth.py`** (tested by
  `tests/test_documentation_truth.py`, 17 unit tests, run via
  `python -m unittest discover -s tests -p 'test_documentation_truth.py'`).
  Runs in CI (`.github/workflows/documentation-truth.yml`, on every PR,
  push to `main`, and weekly). Checks: local Markdown links resolve,
  backtick-quoted repository file references resolve (excluding
  brace-expansion shorthand like `foo.{hpp,cpp}`), this file's "Major
  components" table's empty/implemented directory claims match the actual
  filesystem, documented host-test-function counts match the actual count
  of `void TestXxx(...)` definitions in `tests/host/test_*.cpp` (masking
  fenced code blocks first, so an example value in a `docs/*.md` sample
  JSON block can't be misread as a real claim), documented component
  version strings match `idf_component.yml` (same code-fence masking), an
  unqualified claim that this project lacks CI never survives alongside an
  existing workflow file, no AI-prompt residue leaks into committed docs,
  any future historical/superseded doc carries a visible marker, and the
  provenance policy documents below actually exist and `PROJECT_TRUTH.md`
  still states the IP-safety principle.
- **`tools/check_data_provenance.py`** (tested by
  `tests/test_data_provenance.py`, 20 unit tests) is the sibling checker
  for map-data licensing policy rather than documentation consistency —
  see "Data provenance and attribution" above for what it validates. It
  does not decide what a license means; it enforces decisions already
  recorded in `data/sources/*.json` against `docs/DATA_PROVENANCE_REGISTRY.md`'s
  rules (e.g. `ODBL` sources must have `share_alike_required: true`, no
  source can be `clean_pack_allowed` unless its class is public-domain-
  class, `REVIEW_REQUIRED` sources can never be `official_pack_allowed`).

## Build system

- Repo-root `CMakeLists.txt`: ESP-IDF component registration
  (`idf_component_register`), matching `hardcoreerik/esp-rtl-sdr`'s
  pattern exactly (repo root = component root). Currently registers only
  the portable-core sources that exist (`src/core/geo.cpp`,
  `src/tiles/pmtiles_reader.cpp`, `src/render/style.cpp`, vendored miniz).
- `tests/host/CMakeLists.txt`: separate, standalone CMake project (its own
  `project()` call), building `orcmap_host_tests` directly from source —
  no ESP-IDF toolchain involved. This is deliberate, again matching
  `esp-rtl-sdr`'s precedent, so host tests are always runnable in a plain
  dev environment.
- No ESP-IDF build has been exercised yet (no ESP-IDF toolchain available
  in this development environment) — the component registration is
  structurally correct per ESP-IDF conventions but not yet build-verified
  against a real ESP-IDF SDK. Flagged explicitly rather than claimed as
  proven.

## Dependency inventory

See `docs/DEPENDENCY_LEDGER.md` for the full, authoritative table. Summary:
miniz (tinfl inflate subset only, MIT, vendored under `third_party/miniz`)
is the only runtime dependency. The official `pmtiles` Python package
(BSD-3-Clause) is a dev-only fixture-generation tool, not vendored, not
shipped.

## Performance architecture

Not yet measurable — no renderer, no real pack, no on-device build
exercised. See `docs/PERFORMANCE.md` (placeholder) and `ROADMAP.md`.

## Security / integrity

Pack-level SHA-256 verification is a documented requirement
(`docs/DATA_AND_LICENSING.md`) but not yet implemented in this repo
(OrcSDR's existing `catalog_sync.cpp` already does this for its own
packs — see `docs/ORCMAP1_AUDIT.md` §6 — and is the pattern to generalize,
not reinvent). `PmTilesReader` itself defends against malformed/hostile
archive *structure* (oversized allocations, cyclic leaf directories) but
performs no cryptographic verification of archive contents — that is a
pack-installer-layer concern, not the reader's.

Adjacent but distinct: **legal/IP integrity**, i.e. knowing a pack's
contents were legitimately sourced in the first place, is handled by the
data provenance registry (`data/sources/*.json`,
`tools/check_data_provenance.py`) and, once pack manifests exist,
`sources[].provenance_id` references validated against it
(`docs/PACK_MANIFEST_SCHEMA.md` "Future checker extension"). SHA-256
answers "is this the exact bytes we published"; the provenance registry
answers "were we allowed to publish this at all" — both matter, neither
substitutes for the other.

## Future extension points

- `adapters/esp_idf`, `adapters/m5gfx`: designed for, not yet built.
- External `.orcstyle` files: `MapStyle`'s shape doesn't block this (see
  `docs/STYLING.md`), no loader exists.
- Brotli/zstd tile compression: `Inflate()` currently only implements gzip
  and none (`Compression::kGzip`/`kNone`); `kBrotli`/`kZstd` are defined in
  the enum but return `false` (unsupported) if encountered — flagged for
  whoever builds a pack that uses them.
- HTTP Range `ByteSource`: explicitly anticipated by the `ByteSource`
  interface shape, not implemented.
