# OrcMaps Project Truth

This document is the canonical statement of what OrcMaps is and what has
been decided. It should let a developer or AI agent enter this repository
months from now and know what must not be accidentally re-decided or
broken. If a future prompt or PR conflicts with something stated here,
**call out the conflict explicitly and resolve it deliberately** — do not
silently overwrite project direction, and do not silently write around a
contradiction between this document and the code. See "Document authority"
at the end.

## Project Identity

- Repository: `hardcoreerik/orcmaps` (public, created 2026-09-13).
- Local clone convention: `F:\Ai\OrcMaps`.
- Product/project name: **OrcMaps**.
- Component/namespace: `orcmap` (ESP-IDF component name, C++ namespace
  `orcmap::`). Note the singular — the repo/product is "OrcMaps", the
  in-code namespace is "orcmap". This is intentional, not a typo to fix.
- Pack file extension: `.pmtiles` (standard PMTiles extension — see "Map
  format direction" below; OrcMaps does not invent its own extension).

## Mission

OrcMaps is a reusable **offline map engine for embedded devices**. ESP32-
class hardware is the first target platform, not the only intended one —
the core is written to be portable to other microcontroller-class devices
later, though no non-ESP32 port exists yet (see STATUS.md). It is not
"OrcSDR's map code moved to another repo" — it is a standalone engine that
OrcSDR happens to be the first major application of. Unrelated ESP32
projects should be able to depend on it the same way OrcSDR already depends
on `hardcoreerik/esp-rtl-sdr`.

## Relationship to OrcSDR

- OrcSDR is a consumer, not the owner, of OrcMaps.
- OrcMaps must never depend on OrcSDR headers, types, or build files.
- OrcSDR-specific domain concepts (aircraft, LoRa/Meshtastic nodes, P25
  sites, RF transmitters, bearings, heatmaps, signal detections, broadcast
  stations) must never appear inside OrcMaps' core model. See "Map Data vs
  Rendering vs Overlay Separation".
- OrcSDR will eventually depend on OrcMaps as a version-pinned external
  ESP-IDF component (`idf_component.yml`), exactly like its existing
  `esp-rtl-sdr` dependency. **Not yet wired up** — OrcSDR's branch
  `grok/orcmap1-readable-map` still uses its own in-tree `offline_map.cpp`.
- The prototype OrcMaps supersedes is documented in full in
  `docs/ORCMAP1_AUDIT.md` (ORCMAP1: text-based format, 640-segment/32-label
  caps, plate-carrée projection, hardcoded colors, used by
  `apps/orcsdr-tab5/ui/adsb_dashboard.cpp` and `lora_dashboard.cpp`). It
  proved the UI concept works; its limits are not requirements for OrcMaps.

## Core Design Principles

1. Offline first — a downloaded pack renders with zero network access.
2. One basemap, many applications — base geography is shared; every
   consumer supplies its own overlays.
3. Bounded memory — pack/archive size must never determine RAM usage.
4. Streaming/random access — read only what the current viewport needs.
5. Portable core — no application state inside the engine.
6. M5GFX first, not M5GFX forever — rendering backend and map core are
   separable.
7. Map data is not firmware — packs install/update independently.
8. User data is not map-pack data — markers/waypoints survive pack
   replacement, never leave the device as part of pack discovery.
9. Safe updates — a corrupt/partial download never replaces a working pack.
10. Provenance matters — every pack carries source/license/attribution/hash.
11. No unauthorized tile scraping — packs come from legitimate extract data
    (e.g. OSM `.osm.pbf`), never bulk-scraped raster tile servers.
12. Measure performance — do not optimize or choose formats blindly.
13. Maintain host-application responsiveness — on OrcSDR specifically, SDR
    USB/audio/DSP work takes priority over background map operations; map
    I/O never runs on a real-time thread (inherited constraint from
    ORCMAP1's `load()` contract, `docs/ORCMAP1_AUDIT.md` §2).
14. Don't overengineer the first milestone.

## Current Technology Direction

- **Map pack container: PMTiles v3, decided.** See
  `docs/FORMAT_DECISION.md` for the full evaluation (PMTiles vs. MBTiles
  vs. custom `ORCMAP2`, raster vs. vector vs. hybrid) and citations. Do not
  re-litigate this without new evidence — MBTiles was rejected specifically
  because SQLite's locking/WAL assumptions don't map cleanly onto FAT32/SD,
  and a custom container was rejected because PMTiles already gives
  bounded-memory seek+read with a real ESP32-S3 precedent
  (`yuiseki/m5-cardputer-offgrid-tiny-map`, ~78 GiB whole-planet vector
  PMTiles archive, no PSRAM).
- **Tile content schema: not yet decided** (general OpenMapTiles-style MVT
  vs. a narrower OrcMaps-specific schema). Deferred to measurement against
  the Lane County vertical slice — see `docs/FORMAT_DECISION.md` "Deferred:
  tile content schema".
- **Rendering: vector tiles decoded once, cached as RGB565** (RAM/PSRAM
  first, optional SD-backed cache second) — the hybrid approach, matching
  the yuiseki precedent and ORCMAP1's own informal sprite-cache pattern in
  `adsb_dashboard.cpp`. Not yet implemented (no MVT decoder exists yet).
- **Projection: standard Web Mercator tile math**
  (`orcmap::LatLonToTile`/`TileToLatLon`, `include/orcmap/geo.hpp`),
  implemented and host-tested. RF-specific distance/bearing/geodesic math
  stays in the consuming application (OrcSDR), never in OrcMaps.

## Map Data vs Rendering vs Overlay Separation

```
Geographic map data (.pmtiles)
        |
Base map style (orcmap::MapStyle)
        |
Rendered map (renderer -- not yet implemented)
        |
Generic OrcMaps overlays (marker/icon/text/polyline/polygon/circle/route/
        |                  track/waypoint -- src/overlays, not yet implemented)
Application-specific overlays (aircraft/nodes/RF sites -- OrcSDR's own code)
```

`MapStyle` has, and must continue to have, zero fields for any OrcSDR
concept. See `docs/STYLING.md` "Overlays are NOT base-map styles" — this is
treated as a correctness rule, not a preference.

## Supported / Target Hardware

- Primary initial target: **ESP32-P4 / M5Stack Tab5**, SD-card storage,
  M5GFX rendering, 1280×720 display.
- Secondary feasibility target: ESP32-S3, kept realistic by not letting
  core code assume Tab5-specific resources (PSRAM size, display size).
- The core (`include/orcmap`, `src/core`, `src/tiles`, `src/render/style.cpp`)
  has **zero** ESP-IDF, M5Stack, M5GFX, or LovyanGFX dependency today, and
  must stay that way — this is enforced structurally (those headers are
  never included from `src/core`, `src/tiles`, or the style system) and
  verified by the fact that `tests/host` builds and passes on a plain host
  C++17 toolchain with no ESP-IDF present.

## Storage Model

`orcmap::ByteSource` (`include/orcmap/byte_source.hpp`) is the one seam:
`Read(offset, dest, length)`, `Size()`, `Valid()`. No filesystem, no
ownership of the underlying resource beyond what an adapter documents.
Implemented adapters: `orcmap::host::FileByteSource`
(`adapters/host/file_byte_source.cpp`, stdio-backed, host tests/tools only).
**Not yet implemented:** `adapters/esp_idf` (wrapping OrcSDR's
`orcsdr::storage::FileSystem` or raw ESP-IDF VFS).

## Rendering Model

No renderer exists yet (see STATUS.md). The intended shape, per
`docs/STYLING.md`: a renderer consumes `orcmap::MapPaint` from
`ResolveFeatureStyle()`, never reads style colors or hardcodes literals
directly. `orcmap::Color` is plain RGBA8; conversion to a display's native
pixel format (RGB565 for M5GFX) is an adapter's job (`adapters/m5gfx`, not
yet implemented), not the core's.

## Styling Model

Implemented and host-tested. See `docs/STYLING.md` for the full design.
Four built-in styles exist: `orcsdr-dark`, `standard-light`,
`high-contrast-field`, `night-red-safe` (`src/render/style.cpp`).
`orcmap::DefaultStyle()` is `standard-light` (OrcMaps' own neutral
default) — OrcSDR selects `orcsdr-dark` explicitly at its own integration
point, not inside OrcMaps. Runtime style switching
(`orcmap::StyleManager`) never reloads map-pack data. Rendered-tile cache
keys (`orcmap::RenderedTileCacheKey`, `include/orcmap/cache_key.hpp`) must
include pack id, tile coordinate, style id, style version, and renderer
version — this is enforced by `tests/host/test_style.cpp`.

## Pack Distribution Model

- Not yet implemented on-device. Direction (per `docs/ORCMAP1_AUDIT.md` §6
  and `docs/PACK_FORMAT.md`): pack discovery will be directory-scan based
  (enumerate installed `.pmtiles` files), not the fixed 16-slot table
  OrcSDR's `catalog_sync.cpp` currently uses for its other data types.
- OrcSDR's existing `catalog_sync` streaming-download / atomic-activate
  (`.part` → verify → `.bak` backup → rename)/rollback machinery is the
  proven foundation to generalize for map-scale (multi-GB, chunked,
  resumable) transfers — not to be thrown away and rebuilt from scratch.
- Map data does not live in this source repository. Regional/world packs
  will be distributed via releases/object storage, not committed binaries
  (`tests/fixtures/tiny.pmtiles`, 519 bytes, is a synthetic test fixture,
  not real map data, and is the only binary map-shaped file in this repo).

## Licensing Model

- Engine code: **AGPL-3.0-only**, with commercial licensing available by
  agreement — same model as `hardcoreerik/esp-rtl-sdr` and
  `hardcoreerik/OrcSDR`. See `LICENSE`, `LICENSING.md`. This is the actual
  repository license today (`LICENSE` is the real GNU AGPLv3 text), not
  aspirational.
- This dual-license structure is a deliberate choice to preserve future
  commercial licensing optionality (to manufacturers/proprietary products),
  not a legal claim beyond what the LICENSE file states.

## Data Licensing Rules

See `docs/DATA_AND_LICENSING.md` in full. Key rules:

- Never build packs by bulk-downloading `tile.openstreetmap.org` or any
  other raster tile service — OSM's tile service explicitly prohibits this
  usage for offline archive construction.
- Packs come from legitimate extract data (OSM `.osm.pbf` extracts via
  Geofabrik/planet + osmium/Planetiler/tippecanoe, or another source whose
  terms explicitly permit offline redistribution).
- Every pack must carry source/source_url/source_date/license/attribution/
  generator/generator_version/data_version/sha256 in its metadata.
- Engine license and map-data license are independent. OSM-derived data
  carries ODbL obligations (attribution, share-alike on the *data*) that
  apply regardless of the engine's own license.

## Performance Constraints

Not yet measured (no renderer or real pack exists). `docs/PERFORMANCE.md`
is created but currently a placeholder pending the Lane County vertical
slice. Real numbers from the yuiseki precedent are cited in
`docs/FORMAT_DECISION.md` (z13 tile render 33s→8.1s optimized, z0 0.4s,
~95 KB free heap with no PSRAM) as the bar OrcMaps should beat given
stronger target hardware (PSRAM-equipped ESP32-P4 vs. that project's
no-PSRAM ESP32-S3).

## Memory Constraints

`PmTilesReader` holds only the 127-byte header and the decompressed root
directory (≤16,257 bytes compressed per the PMTiles v3 spec) after
`Open()`. Leaf directories and tile bytes are fetched on demand into
caller-owned buffers, never accumulated. This is implemented and verified
by `tests/host/test_pmtiles.cpp` reading a real (synthetic) archive.

## Compatibility Goals

- A properly-licensed `.pmtiles` archive built by any standard tool
  (Planetiler, tippecanoe, `pmtiles` CLI) should be a valid OrcMaps input,
  not just archives built by OrcMaps' own (not-yet-built) pack builder.
- ESP-IDF ≥5.0 (`idf_component.yml`).

## Local Development Conventions

- All local branches/worktrees for OrcMaps development are created under
  `F:\Ai\OrcMaps-Temp\` — the same pattern OrcSDR already uses
  (`F:\Ai\OrcSDR-Temp\`). The primary clone stays at `F:\Ai\OrcMaps`; a
  worktree for branch `foo` goes at `F:\Ai\OrcMaps-Temp\OrcMaps-foo` (or
  similar), never scattered elsewhere.
- **Documentation Truth CI is a project rule, not optional tooling.**
  `.github/workflows/documentation-truth.yml` runs
  `tools/check_documentation_truth.py` on every PR, push to `main`, and
  weekly on a schedule — adapted directly from OrcSDR's own
  `documentation-truth.yml`/`tools/check_documentation_truth.py`, which is
  explicitly credited by the user as part of what has made that project's
  workflow work well. Every durable-doc edit (`PROJECT_TRUTH.md`,
  `ARCHITECTURE.md`, `ROADMAP.md`, `STATUS.md`, `README.md`,
  `LICENSING.md`, `docs/*.md`) should be checked locally
  (`python tools/check_documentation_truth.py` from the repo root) before
  committing — the same discipline as running host tests before pushing.
  See "Document authority" at the end of this file for why this matters:
  the checker exists specifically to catch the gap between what a doc
  claims and what the repository actually contains (it already caught and
  this session fixed a real drift: a test-count claim that said 17 when
  the actual count was 20).

## Naming Conventions

- Files/types: `PascalCase` for types (`PmTilesReader`, `MapStyle`),
  `PascalCase` for free functions (`ResolveFeatureStyle`, `ZxyToTileId`) —
  matches the Google-style C++ convention already used in this codebase,
  not `snake_case` (note: this differs from OrcSDR's own `offline_map.cpp`,
  which uses `snake_case` — OrcMaps does not have to match OrcSDR's C++
  style, only its licensing/distribution pattern).
- `k`-prefixed constants (`kMercatorMaxLatDeg`, `kRendererVersion`).
- Style ids: lowercase-kebab-case strings (`orcsdr-dark`).

## Decisions Already Made

- Repo: `hardcoreerik/orcmaps`, public, AGPL-3.0.
- Local path convention: `F:\Ai\OrcMaps`.
- Container format: PMTiles v3 (see above).
- Rendering approach: vector-decode-once + RGB565 cache (hybrid).
- Style system: 4 built-in compiled styles, `MapStyle`/`FeatureRule`/
  `ResolveFeatureStyle` API, `RenderedTileCacheKey` includes style
  id+version+renderer version.
- Dependency: miniz (tinfl inflate subset), MIT, vendored under
  `third_party/miniz`, used only for gzip/deflate inflate of PMTiles
  directories/tiles. Recorded in `docs/DEPENDENCY_LEDGER.md`.
- Test fixture built via the official `pmtiles` Python package (BSD-3-
  Clause, dev-tool only, not vendored) rather than hand-rolled binary data —
  see `tests/fixtures/generate_fixture.py`.
- CMake layout matches `esp-rtl-sdr`'s precedent exactly: repo-root
  `CMakeLists.txt` is the ESP-IDF component registration file;
  `tests/host/CMakeLists.txt` is a separate standalone host-only CMake
  project (no ESP-IDF toolchain required to build/run tests).

## Explicit Non-Goals

- OrcMaps will not know what an aircraft, LoRa node, RF site, or any other
  OrcSDR concept is.
- OrcMaps will not implement payment/license enforcement (commercial
  licensing stays a manual agreement process).
- OrcMaps will not require every user to install a full-planet pack —
  tiered packs (world/country/state/local) are the goal, sharing one
  rendering path.
- OrcMaps will not casually invent `ORCMAP2` — see "Current Technology
  Direction" above.

## Open Decisions

- Tile content schema (general OpenMapTiles-style MVT vs. narrower custom
  schema) — deferred to Lane County vertical-slice measurement.
- Exact zoom tiers/sizes per pack tier (world/country/state/local) — needs
  a real built pack to measure; published planet-scale numbers found in
  research (~120 GB Protomaps z0-15, ~78 GiB yuiseki z0-14) are not precise
  enough to derive Oregon/county-scale numbers from.
- Multi-file/chunked archive scheme for packs exceeding FAT32's 4 GiB
  single-file limit — not yet designed (yuiseki's 2 GiB-chunk precedent is
  the closest reference).
- External style file format (`.orcstyle`) — shape not designed, though the
  `MapStyle` struct is already serializable-shaped for it later.

## Historical Context That Matters

ORCMAP1 (`docs/ORCMAP1_AUDIT.md`) proved the UI/integration pattern:
`draw_base()` into a cached sprite (ADS-B) or direct-to-display (LoRa),
overlays drawn by the consuming dashboard using the map's own
`project()`/`View` for placement (LoRa) or independent polar math (ADS-B).
This pattern — base map draws once, overlays are a separate pass, style
lives in the caller — is exactly what OrcMaps formalizes. It is not being
discarded; it is the validated precedent this project is built from.

## Document Authority

When a durable decision changes — architecture, requirement, constraint,
naming, technology selection/rejection — **update this file as part of that
change**, not as an afterthought. Do not let an important design decision
exist only in a chat transcript, a commit message, or an AI agent's memory.
If a future prompt conflicts with something stated here, surface the
conflict explicitly before proceeding, and update this document
deliberately if the conflict resolves in favor of the new direction —
never silently overwrite it, and never silently work around the
contradiction in code while leaving this document stale.
