# OrcMaps Roadmap

Where we're going and in what order. Checkboxes reflect actual repository
state as of 2026-09-13 (see `STATUS.md` for the live snapshot; this file
changes on phase completion/priority shifts, not every session).

## Current objective

Get one real map pack (Lane County) rendering on Tab5 hardware through
OrcMaps, end to end: PMTiles archive on SD → decoded vector tile → styled
draw call → pixels on screen → pan/zoom. Nothing else matters more than
this until it's true, per the project's own "don't overengineer the first
milestone" principle (`PROJECT_TRUTH.md`).

## Phase 0 — Repository / architecture foundation

**Goal:** stand up the standalone repo with a real, evidence-based format
decision and a portable, host-tested core.

- [x] Repository created (`hardcoreerik/orcmaps`, public, AGPL-3.0)
- [x] Directory scaffold, LICENSE, LICENSING.md, README
- [x] ORCMAP1 audit (`docs/ORCMAP1_AUDIT.md`) — what to keep/replace
- [x] Format research: PMTiles v3 spec, MBTiles, yuiseki ESP32-S3 precedent
- [x] Format decision recorded with citations (`docs/FORMAT_DECISION.md`)
- [x] Data/licensing policy (`docs/DATA_AND_LICENSING.md`), dependency
      ledger (`docs/DEPENDENCY_LEDGER.md`)
- [x] Four-document project-memory system (this file + `PROJECT_TRUTH.md` +
      `ARCHITECTURE.md` + `STATUS.md`)

**Exit criteria:** met.

## Phase 1 — Map format vertical slice

**Goal:** prove the chosen container format works, in code, against a real
(if synthetic) archive — not just on paper.

- [x] `orcmap::ByteSource` abstraction + host `FileByteSource` adapter
- [x] `orcmap::PmTilesReader`: header parse, directory parse (gzip via
      vendored miniz), Hilbert tile-ID addressing, leaf-directory
      indirection, tile byte-range lookup
- [x] Deterministic, small, committed test fixture
      (`tests/fixtures/tiny.pmtiles`, 519 bytes) built via the official
      reference `pmtiles` Python library, not hand-rolled
- [x] Host tests: header fields, byte-exact tile content, sparse/missing
      tiles, corrupt-archive handling, Hilbert ID correctness — all passing
- [ ] Same proof against a **real** OSM-derived `.pmtiles` archive (not the
      synthetic fixture) — not yet done, blocks Phase 4

**Exit criteria:** met for the synthetic-archive proof; real-archive proof
deferred to Phase 4 (needs an actual OSM extract pipeline, Phase 4's job).

**Risks:** the synthetic fixture doesn't exercise leaf-directory
indirection (the fixture's directory fits in the root) or gzip-compressed
*tile* payloads (only the directory is gzip-compressed in the fixture,
tile payloads are stored uncompressed) — a real archive may exercise code
paths the current tests don't. Flagged, not yet mitigated.

## Phase 2 — Rendering foundation

**Goal:** decode and draw one real tile.

- [x] Geo/tile math (`geo.hpp`/`geo.cpp`), Web Mercator, host-tested,
      including a real bug (unsigned underflow at the Mercator latitude
      limit) caught and fixed by tests
- [x] Style system (`style.hpp`, `color.hpp`, `cache_key.hpp`,
      `style.cpp`) — done ahead of the original phase order because it was
      explicitly requested mid-session; does not block the items below, but
      means "Style System" is no longer its own later phase (folded in here)
- [ ] MVT (vector tile) decoder — **not started, the current critical path**
- [ ] `adapters/esp_idf` `ByteSource` (wraps OrcSDR's
      `orcsdr::storage::FileSystem` or raw ESP-IDF VFS)
- [ ] `adapters/m5gfx` renderer backend (Color → RGB565, draw calls against
      `lgfx::v1::LovyanGFX&`, matching ORCMAP1's existing portable-overload
      pattern — see `docs/ORCMAP1_AUDIT.md` §4)
- [ ] Renderer core (`src/render/`) that walks decoded MVT features, calls
      `ResolveFeatureStyle()`, issues draw calls
- [ ] First on-device ESP-IDF build of the OrcMaps component (not yet
      exercised at all — no ESP-IDF toolchain used so far)

**Dependencies:** Phase 1 (reader) must keep working; style system (done)
feeds the renderer once it exists.

**Exit criteria:** one real tile's geometry, decoded from a `.pmtiles`
archive, drawn to a Tab5 (or host-side test render) using an active
`MapStyle`, with a measured decode+render time recorded in
`docs/PERFORMANCE.md`.

## Phase 3 — Lane County / Eugene proof

**Goal:** the actual vertical slice success criteria from the original
project brief — pan, zoom, markers, ADS-B/LoRa overlays, bounded memory,
OrcSDR still builds.

- [ ] Real Lane County `.pmtiles` archive built from an OSM `.osm.pbf`
      extract (Geofabrik or similar) via a proper tiler (Planetiler/
      tippecanoe) — not the Overpass-JSON or incomplete-GeoJSON scripts
      documented in `docs/ORCMAP1_AUDIT.md` §8
- [ ] Pan works (viewport moves, new tiles load, cache reused where
      possible)
- [ ] Zoom works (tile set changes correctly across zoom levels)
- [ ] A generic marker overlay draws correctly at a lat/lon
- [ ] Memory measured and bounded (peak RAM, peak PSRAM logged)
- [ ] OrcSDR still builds and runs, unmodified, throughout (OrcMaps
      development must never break the working branch —
      `PROJECT_TRUTH.md` principle 13/`docs/ORCMAP1_AUDIT.md`)

**Dependencies:** Phase 2 renderer working.

## Phase 4 — OrcSDR integration

**Goal:** OrcSDR consumes OrcMaps as an external dependency; ADS-B and LoRa
basemaps migrate off `offline_map.cpp`.

- [ ] OrcSDR's `idf_component.yml` gains a version-pinned dependency on
      `hardcoreerik/orcmaps`
- [ ] ADS-B basemap migrated (preserve the existing sprite-cache pattern in
      `adsb_dashboard.cpp`; ADS-B keeps owning aircraft state and its own
      polar-math overlay placement — see `docs/ORCMAP1_AUDIT.md` §5)
- [ ] LoRa basemap migrated (preserve `follow_selected_position()` and
      the existing use of the map's own projection for node placement)
- [ ] `offline_map.cpp`/`.hpp` deleted **only after** both migrations are
      demonstrated working — never before
- [ ] OrcSDR Dark style set as OrcSDR's default at its integration point
      (`map.SetStyle(orcmap::BuiltinStyle::kOrcSdrDark)`), per
      `docs/STYLING.md`

## Phase 5 — Full-screen map application

**Goal:** OrcSDR gets a dedicated, generic full-screen map browser.

- [ ] Drag-to-pan, touch zoom
- [ ] Current center/zoom display, center-on-receiver, center-on-selected
- [ ] Marker placement (tap/long-press), naming, delete, recenter
- [ ] Visible attribution, pack info
- [ ] Graceful "no data for this area/zoom" state
- [ ] Map Style switcher wired into Settings → Maps (per
      `docs/STYLING.md` "OrcSDR-specific styling guidance" — deferred until
      now specifically because the ask was to get the underlying API right
      first, not to build UI early)

## Phase 6 — Pack management

**Goal:** install/update/remove map packs, both by SD copy and Wi-Fi.

- [ ] On-device pack discovery: directory scan of installed `.pmtiles`
      files (not the fixed-slot catalog table pattern — see
      `docs/PACK_FORMAT.md`)
- [ ] Generalize OrcSDR's `catalog_sync` streaming/atomic-activate/rollback
      machinery for map-pack scale (chunked, resumable, per-chunk hash)
- [ ] Settings → Data & Maps → Maps UI: Installed/Available/World/regions
      tree, size/coverage/zoom display before download
- [ ] `orcsdr_storage::used_bytes()` fixed (currently stubbed to always
      return 0 — `docs/ORCMAP1_AUDIT.md` §7) so real free-space checks work
      before large map downloads trust them

## Phase 7 — Regional scale testing

**Goal:** prove the same code path scales past one county.

- [ ] Oregon / Pacific Northwest pack built and benchmarked
- [ ] Tile-boundary crossing tested heavily (pan across many tiles)
- [ ] Performance numbers recorded in `docs/PERFORMANCE.md`

## Phase 8 — Global / planet scale

**Goal:** evaluate, don't assume, whole-world feasibility.

- [ ] World base pack (low/medium zoom) built and benchmarked
- [ ] Full-planet pack evaluated against real hardware limits (SD card
      size, FAT32 4 GiB file limit → chunking scheme, per yuiseki precedent)
- [ ] Document actual limits found, not estimated ones

## Phase 9 — Ecosystem / external applications

**Goal:** prove OrcMaps is reusable beyond OrcSDR.

- [ ] `examples/generic-esp32` — a minimal, non-OrcSDR consumer
- [ ] Stable, tagged OrcMaps releases (semver, `idf_component.yml` version
      bumps) that OrcSDR (and, ideally, at least one unrelated project)
      pin against

## Deferred work

- Tile content schema decision (general MVT vs. narrower custom) —
  deferred to Phase 3 measurement, see `docs/FORMAT_DECISION.md`.
- External `.orcstyle` style files — not blocked, not started.
- Brotli/zstd tile compression support in `PmTilesReader::Inflate()`.
- HTTP Range `ByteSource` (remote streaming).
- Label collision avoidance / priority rendering.
- Payment/commercial-license enforcement — explicit non-goal for now, see
  `PROJECT_TRUTH.md`.

## Research questions

- What tile content schema minimizes on-device MVT decode cost without
  requiring a fully custom tiler? (Phase 3)
- What's the actual Lane County / Oregon `.pmtiles` size at a practical
  zoom range? No credible published number exists for these — must be
  measured directly (`docs/FORMAT_DECISION.md`).
- Does the ESP32-P4 + PSRAM Tab5 target comfortably beat the yuiseki
  no-PSRAM ESP32-S3 numbers, or does BLE/Wi-Fi coexistence impose similar
  heap-fragmentation constraints? (Phase 2/3)

## Release milestones

No tagged release yet. First candidate: `v0.1.0` once Phase 2 exit
criteria are met (one real tile decoded, styled, and drawn) — this file's
own `idf_component.yml` already declares `version: "0.1.0"` as the
in-development version, not yet a tagged release.
