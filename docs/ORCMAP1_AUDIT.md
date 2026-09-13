# ORCMAP1 Audit — starting point for OrcMaps

This documents the existing map implementation in `hardcoreerik/OrcSDR` (branch
`grok/orcmap1-readable-map`) as of 2026-09-13, before extraction into this
standalone repository. It is the baseline OrcMaps must equal or exceed for the
Lane County vertical slice, and the source of constraints (memory budgets,
concurrency rules, storage API shape) OrcMaps inherits.

Source files audited, all in the `hardcoreerik/OrcSDR` repository (none of
these paths exist in this OrcMaps repository):
*apps/orcsdr-tab5/ui/offline_map.{hpp,cpp}*,
*adsb_dashboard.cpp*, *lora_dashboard.cpp*, *catalog_sync.{hpp,cpp}*,
*orcsdr_storage.{hpp,cpp}*, *wifi_service.{hpp,cpp}*, *settings_app.{hpp,cpp}*,
*tools/data_catalog/**, *docs/DATA_CATALOG.md*, *docs/DATA_SOURCE_LEDGER.md*,
*LICENSING.md*.

## 1. ORCMAP1 format (what exists today)

Plain-text, line-oriented, path `/orcsdr/data/lane_county_map.idx`:

- Header: literal `ORCMAP1` (parser compares 7 bytes read via
  `readBytesUntil('\n', ...)`; writers emit `ORCMAP1\n`).
- One record per line, max 95 chars:
  - Segment: `%c %f %f %f %f` → `kind lat1 lon1 lat2 lon2`, `kind ∈ {R,W,A}`
    (road/water/airport). Lat validated to [-90,90], lon to [-180,180].
  - Label: `L %f %f %23[^\n]` → `lat lon text` (≤23 chars).
- Hard caps: **640 segments, 32 labels** (`kSegmentCapacity`,
  `kLabelCapacity`), enforced both on-device and by the build scripts.
- No metadata: no version, bounds, license, or provenance fields in the file
  itself — attribution (`"OSM contributors"`) is hardcoded into the renderer,
  not the pack. No zoom levels — it's a single fixed-detail dataset.

## 2. Storage / memory

- Two fixed-size arrays, `Segment g_segments[640]` (17 bytes each) and
  `Label g_labels[32]`, declared `EXT_RAM_BSS_ATTR` (PSRAM), populated once by
  `load()` and held for process lifetime. ~15 KB total. No streaming, no
  paging — the whole pack is memory-resident after load.
- `load()` is documented as UI-thread-only, never called from SDR/audio
  callbacks — a concurrency contract OrcMaps must preserve explicitly in its
  API (e.g. all pack I/O happens off any real-time thread).
- No on-disk rendered-tile cache; the only cache is an **application-side**
  `M5Canvas` sprite in `adsb_dashboard.cpp` keyed on
  `(lat_e7, lon_e7, range_nm)` — i.e. caching already happens one layer above
  `offline_map`, which validates the OrcMaps design of putting a cache
  between the engine and the renderer rather than inside the raw parser.

## 3. Projection

Plate carrée (equirectangular), not Mercator, no tiles, no zoom levels:

```
lon_scale = max(0.1, cos(center_lat_rad))
east_nm  = (lon - center_lon) * lon_scale * 60.0
north_nm = (lat - center_lat) * 60.0
x = view.x + w/2 + east_nm  * (w/2) / range_nm
y = view.y + h/2 - north_nm * (h/2) / range_nm
```

Single linear scale per draw call, centered on the viewport, driven by a
`range_nm` half-extent. Adequate for a 25 nm radar view; not adequate as the
primary projection for a tiled global map (no notion of tile boundaries,
world wrap, or standard zoom levels). Web Mercator tile math is a genuinely
new subsystem, not a refactor of this code.

Cohen-Sutherland line clipping against the viewport precedes every draw call
(`clip_code`/`clip_line`) — this clipping logic is reusable as-is for
segment/line rendering regardless of projection.

## 4. Rendering

`draw_base()` takes a `lgfx::v1::LovyanGFX&` reference (the portable
LovyanGFX interface, not `M5.Display` directly — good) plus caller-supplied
colors for water/road/airport/border. **Style lives entirely in the caller**;
the pack carries zero color/style information. This decoupling should be
preserved and formalized as an explicit style object in OrcMaps.

Segments draw as `drawLine`, water/airport get a 1px-offset second line to
fake 2px thickness. Labels draw via `setTextDatum`/`drawString`, gated by
`project()` returning true (i.e., in-viewport). No label collision avoidance,
no priority — all 32 labels always attempt to draw if in view.

A convenience `draw_base(view, ...)` overload defaults to `M5.Display`,
coupling that one call site to M5Unified globally; the `LovyanGFX&` overload
is the portable one and is what both dashboards actually use.

## 5. How ADS-B and LoRa consume it — map/overlay separation already exists

This is the most important finding for the OrcMaps API design: **the
separation between base map and overlay is already real in practice**, even
though there's no formal `Overlay` interface yet.

- **ADS-B** (`adsb_dashboard.cpp`): calls `offline_map::draw_base()` once
  into a cached `M5Canvas` sprite (re-rendered only when
  lat/lon/range change), then composites aircraft **separately**, using its
  own **polar** math relative to radar center (bearing/range from haversine),
  not `offline_map::project()`. Map and aircraft are two independent draw
  passes on the same canvas. offline_map never sees aircraft state.
- **LoRa** (`lora_dashboard.cpp`): calls `offline_map::draw_base()` directly
  to `M5.Display` (no sprite cache), then draws node markers using
  `offline_map::project(view, node.lat, node.lon, &x, &y)` — i.e. it reuses
  the map's own lat/lon→screen projection for overlay placement. offline_map
  never sees node state, names, or selection.

Conclusion: an `Overlay`/`Marker` API that takes lat/lon and returns/consumes
screen coordinates via the engine's projection (as LoRa already does
informally) is a natural, low-risk extraction — the pattern already exists,
it just needs a formal interface and to also support the ADS-B case (polar
overlays that want the *view's* screen-space math but not necessarily the
tile projection, e.g. range rings) as a documented alternative overlay style.

## 6. Signed catalog / install infrastructure (`catalog_sync`)

Already a solid foundation for map-pack distribution, generalizable rather
than replaceable:

- Manifest `catalog-v1.json` + detached signature `catalog-v1.sig`
  (P-256, `mbedtls_pk_verify`, trust anchor baked into firmware at build
  time), fetched from GitHub Releases, max 16 KB, SHA-256'd.
- Per-artifact streaming download: `esp_http_client` → 4 KB chunks → PSRAM
  batch buffer → SD write, incremental SHA-256, free-space check before
  starting, `vTaskDelay(1)` every 64 KB to avoid starving other tasks.
- Atomic-ish activation: download to `<dest>.part` → verify hash/format →
  rename current `<dest>` to `<dest>.bak` → rename `.part` into place → on
  any failure, rollback from `.bak`. Version tracked via a `<dest>.ver`
  sidecar file, compared against manifest version for update detection.
- Runs on a dedicated pinned FreeRTOS task (core 1, priority 3), state
  guarded by a spinlock, UI polls a snapshot — never touches the SDR/audio
  path.
- **Gaps for map-scale packs** (multi-GB, chunked, resumable): no HTTP Range
  request support anywhere (`wifi_service.cpp` has no HTTP client at all —
  all HTTP lives in `catalog_sync.cpp`); a failed download restarts from
  byte 0, not from `.part`'s current length; no per-chunk hashing, only a
  whole-artifact SHA-256 computed incrementally during one continuous
  download. `orcsdr_storage::used_bytes()` is stubbed to always return 0, so
  free-space checks today rely on an externally-supplied value, not a real
  filesystem query — needs fixing before large map downloads can trust it.
- Fixed-size pack table (`kPackCount = 16`, 5 built-in + P25 slots) with a
  hardcoded per-pack-ID format-sniff switch in `download_artifact()` — not
  designed for an open-ended, growing catalog of map packs (World / country /
  state / local tiers). OrcMaps' pack discovery needs to be directory-scan
  based (enumerate `/orcsdr/maps/*.pmtiles` or similar) rather than living in
  this fixed table, with the *download* side still reusing catalog_sync's
  proven streaming/verify/atomic-activate machinery, generalized for
  variable-length chunked transfer and resume.

## 7. Storage abstraction (`orcsdr_storage`)

Thin wrapper over ESP32 SDMMC/FAT VFS (stdio `FILE*`/`DIR*` under an
Arduino-`File`-shaped API for source compatibility). Has `seek()` (absolute
position only, no whence) + `read()`, so byte-range reads are *possible* by
composition today, but there's no dedicated `read(offset, buf, len)` helper
and nothing exercises it that way yet — both `offline_map` and `catalog_sync`
do full sequential reads. This is exactly the gap OrcMaps' `ByteSource`
abstraction (`read(offset, dest, length)`, `size()`, `valid()`) fills, with
an adapter wrapping this exact `orcsdr::storage::FileSystem` for the OrcSDR
integration.

## 8. Build pipeline (OrcSDR's *tools/data_catalog/build_lane_county_map.py*, x2)

Two **different, incomplete-alone** scripts, not one tool duplicated (both
in the OrcSDR repository, not this one):

- *tools/data_catalog/build_lane_county_map.py*: consumes raw **Overpass API
  JSON** directly (not a proper `.osm.pbf` extract), does its own
  Ramer-Douglas-Peucker simplification and priority-based greedy packing into
  the 640-segment budget, but has **10 hardcoded labels** — no real label
  extraction from OSM data.
- `apps/orcsdr-tab5/tools/build_lane_county_map.py`: consumes pre-simplified,
  pre-classified GeoJSON (an unspecified/missing upstream step produces it),
  does no simplification itself, pulls labels dynamically from GeoJSON point
  features, fails loudly (raises) instead of silently truncating on
  over-budget input, and writes atomically (`.tmp` + `replace()`).

Neither is a complete "real OSM extract → pack" pipeline. OrcMaps' pack
builder should be one real pipeline built on a proper OSM extract source
(Geofabrik `.osm.pbf` or similar via osmium/Planetiler), not live Overpass
queries (rate-limited, not intended for bulk/reproducible builds).

## 9. Licensing precedent

`hardcoreerik/OrcSDR` is dual-licensed AGPL-3.0-only / commercial-by-agreement.
`hardcoreerik/esp-rtl-sdr` already follows exactly this pattern as a
standalone, version-pinned dependency: same dual license, own repo, own
releases. **OrcMaps repeats this established pattern** rather than inventing
a new one. OrcSDR's *docs/DATA_CATALOG.md* / *docs/DATA_SOURCE_LEDGER.md* already
treat maps as a *separate governance track* from the other catalog data
("Maps are imported and validated separately... require a separate rights
and format review") — i.e. current docs already anticipate maps not living
inside the same fixed catalog model as FAA/NOAA/FCC packs, which supports
factoring OrcMaps out independently now.

## What OrcMaps inherits vs. replaces

| Keep / generalize | Replace |
|---|---|
| Style-lives-in-caller decoupling (draw_base color params) | Plate-carrée-only projection → add Web Mercator tile math |
| Overlay-via-projection pattern (LoRa's use of `project()`) | Fixed 640/32 record caps → streaming, bounded-memory tile reads |
| Cohen-Sutherland clipping code (reusable) | Text line format with no metadata → self-describing pack + manifest |
| catalog_sync's streaming download / atomic activate / rollback machinery, generalized | Fixed 16-slot pack table + hardcoded format-sniff switch → directory-scan pack discovery |
| `ByteSource`-shaped seek+read already implicit in `orcsdr_storage` | Whole-artifact-only hashing → chunked/resumable large-file transfer |
| UI-thread-only I/O contract | Overpass-JSON / incomplete-GeoJSON build scripts → one real `.osm.pbf`-based pipeline |
