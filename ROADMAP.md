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
- [x] Documentation Truth CI (`.github/workflows/documentation-truth.yml` +
      `tools/check_documentation_truth.py`, adapted from OrcSDR's own,
      with unit tests in `tests/test_documentation_truth.py`)
- [x] IP/provenance safety model recorded as a durable principle in
      `PROJECT_TRUTH.md`, with code-vs-data-license separation formalized
      architecturally (`include/orcmap/attribution.hpp`,
      `include/orcmap/map_source.hpp`)
- [x] Data-license classification system + Clean/Permissive/Open map-pack
      classes defined (`docs/DATA_AND_LICENSING.md`,
      `docs/DATA_PROVENANCE_REGISTRY.md`)
- [x] Data provenance registry (`data/sources/*.json`, 9 starter records)
      + Data Provenance Truth CI
      (`.github/workflows/data-provenance.yml` +
      `tools/check_data_provenance.py`, `tests/test_data_provenance.py`)
- [x] `CONTRIBUTING.md` with a DCO-style contribution/relicensing policy
- [x] Consumer integration test (`tests/consumer/`) proving a public-
      headers-only build works, structurally enforced via CMake include
      visibility
- [x] Pack manifest schema documented (`docs/PACK_MANIFEST_SCHEMA.md`,
      direction only — no pack builder exists to implement it against yet)

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
- [x] MVT (vector tile) decoder — schema-agnostic container decode
      (`orcmap::DecodeMvtTile()`), hand-rolled minimal protobuf reader,
      host-tested against a real MVT fixture. **Not yet done: mapping
      decoded features to `orcmap::FeatureKind` for rendering** — that's
      the tile-content-schema decision (`docs/FORMAT_DECISION.md`
      "Deferred"), still open, and now the actual next blocker for a
      renderer rather than the decoder itself
- [x] Bounded tile-payload decompression (`DecompressPayload`, kNone/kGzip).
      Brotli/zstd return false. GetTile remains raw stored bytes.
- [ ] `adapters/esp_idf` `ByteSource` (wraps OrcSDR's
      `orcsdr::storage::FileSystem` or raw ESP-IDF VFS)
- [x] OrcMaps-owned Feature / Geometry model, and an explicit MVT →
      OrcMaps translation boundary (`feature.hpp`, `TranslateMvtToFeatureTile`).
      MVT types must not become the renderer architecture. **EXPERIMENTAL**
      FeatureKind heuristic exists; the real schema is still deferred.
- [x] Graphics-independent renderer seam: immediate `RenderTarget` +
      `RenderFeatureTile` + host framebuffer proof. M5GFX consumes this
      seam via `DisplayTarget`.
- [x] Minimal Viewport (prepared `TileScreenMap`). PARTIAL: no overzoom,
      no visible-tile enumerator, no antimeridian wrap. Zoom 0..31.
- [x] `adapters/m5gfx` `DisplayTarget` (`RenderTarget` over
      `lgfx::v1::LovyanGFX&`, Color→RGB565, shared line clip). No MVT.
      `examples/m5gfx` is an ESP-IDF compile proof, not a Tab5 product
      demo.
- [ ] Tile content schema decided (`docs/FORMAT_DECISION.md` "Deferred").
      Springfield / 97477 OpenMapTiles 3.16 tiles are now MEASURED host
      evidence; the mapping in `experimental::AssignFeatureKinds` is still
      EXPERIMENTAL and must not be treated as the schema.
- [x] Renderer core (`src/render/renderer.cpp`) that walks OrcMaps
      features (not `MvtFeature`), calls `ResolveFeatureStyle()`, issues
      generic draw operations. Host proof only; not a full map engine.
- [x] Minimal framework-independent Viewport (prepared TileScreenMap).
      PARTIAL: no overzoom / wrap / visible-tile set.
- [x] First ESP-IDF *compile/link* of the OrcMaps component:
      `examples/generic-esp32` built against ESP-IDF 6.0.2, target
      `esp32p4`. Smoke test only (geo + style log lines). Not on-device
      functional validation, not a graphics demo, not in CI.

**Dependencies:** Phase 1 (reader) must keep working; style system (done)
feeds the renderer once it exists.

**Exit criteria:** host-side real-tile render + measured times: **met**
(Springfield / 97477, 1280×720, `docs/PERFORMANCE.md`). Tab5 / SD path
is **not** met.

## Phase 3 — Lane County / Eugene proof

**Goal:** the actual vertical slice success criteria from the original
project brief — pan, zoom, markers, ADS-B/LoRa overlays, bounded memory,
OrcSDR still builds.

- [x] HOST-ONLY evidence: Springfield / 97477 gzip PMTiles from Geofabrik
      Oregon via Planetiler 0.10.2, rendered 1280×720 through the real
      engine (`tools/pack-inspect`, `docs/PERFORMANCE.md`). Not a Lane
      County product pack, not on-device, schema not frozen.
- [ ] Real Lane County `.pmtiles` archive built from an OSM `.osm.pbf`
      extract (Geofabrik or similar) via a proper tiler (Planetiler/
      tippecanoe) — not the Overpass-JSON or incomplete-GeoJSON scripts
      documented in `docs/ORCMAP1_AUDIT.md` §8
- [ ] Real pack manifest produced alongside it, following
      `docs/PACK_MANIFEST_SCHEMA.md`, referencing the `openstreetmap`
      provenance record (already `CONFIRMED`/approved) by id
- [ ] `tools/check_data_provenance.py` extended to validate manifest
      `sources[].provenance_id` references, per `docs/PACK_MANIFEST_SCHEMA.md`
      "Future checker extension" — there is nothing to check until this
      phase produces a real manifest
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
      `hardcoreerik/orcmaps`, in the exact `git:` URL + full commit-SHA
      shape already used for `esp_rtl_sdr` (`PROJECT_TRUTH.md` "Public API
      and Versioning") — never a branch or floating tag
- [ ] `tests/consumer/` extended/mirrored as needed once OrcSDR is the
      real consumer, so drift between "what the smoke test proves" and
      "what OrcSDR actually needs" gets caught before it becomes an
      integration surprise
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

**Note (resolved 2026-09-13, previously recorded here as a risk):**
OrcMaps' declared ESP-IDF floor is `>=5.0` — deliberately broad, so it
does not block on OrcSDR's own `>=5.5.0,<5.6.0` pin. See
`PROJECT_TRUTH.md` "Compatibility Goals" for the distinction between that
floor and the newer toolchain (latest ESP-IDF 6.x) OrcMaps is actually
developed/tested against. No ESP-IDF version conflict currently blocks
this phase on that basis.

## Phase 5 — Full-screen map application

**Goal:** OrcSDR gets a dedicated, generic full-screen map browser.

- [ ] Drag-to-pan, touch zoom
- [ ] Current center/zoom display, center-on-receiver, center-on-selected
- [ ] Marker placement (tap/long-press), naming, delete, recenter
- [ ] Visible attribution, pack info — consumes
      `orcmap::CollectRequiredAttribution()` (`include/orcmap/map_source.hpp`,
      already implemented/tested) once real `MapSourceInfo` values exist
      to feed it
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
- [x] Host performance numbers recorded in `docs/PERFORMANCE.md` (Springfield
      97477). ESP32 numbers still missing.

## Phase 8 — Global / planet scale

**Goal:** evaluate, don't assume, whole-world feasibility.

- [ ] World base pack (low/medium zoom) built and benchmarked
- [ ] Full-planet pack evaluated against real hardware limits (SD card
      size, FAT32 4 GiB file limit → chunking scheme, per yuiseki precedent)
- [ ] Document actual limits found, not estimated ones

## Phase 9 — Ecosystem / external applications

**Goal:** prove OrcMaps is reusable beyond OrcSDR.

- [x] `examples/generic-esp32` — ESP-IDF compile/link smoke test of the
      portable core with **no** M5GFX/M5Unified dependency (proves a
      generic ESP-IDF app can consume OrcMaps). Not a map-rendering demo.
- [ ] A fuller non-OrcSDR example that actually opens a pack / draws
      (graphics-specific examples stay separate; Tab5 stays in
      `examples/m5stack-tab5`)
- [ ] Stable, tagged OrcMaps releases (semver, `idf_component.yml` version
      bumps) that OrcSDR (and, ideally, at least one unrelated project)
      pin against

## Deferred work

- Tile content schema decision (general MVT vs. narrower custom) —
  still deferred. Springfield OpenMapTiles 3.16 tiles are measured host
  evidence, not a freeze. See `docs/FORMAT_DECISION.md`.
- External `.orcstyle` style files — not blocked, not started.
- Brotli/zstd tile compression support in `DecompressPayload()`.
- HTTP Range `ByteSource` (remote streaming).
- Label collision avoidance / priority rendering.
- Payment/commercial-license enforcement — explicit non-goal for now, see
  `PROJECT_TRUTH.md`.
- Resolve the `REVIEW_REQUIRED` provenance records: directly fetch and
  quote the Census TIGER/Line TechDoc's attribution clause, re-fetch
  NOAA ETOPO's ISO metadata "use constraints" page (503 during research),
  re-fetch the USDOT NAD disclaimer page (403 during research, twice),
  decide the Overture Places per-record source-filtering approach, and
  split `usgs-national-map`'s placeholder record into narrower
  dataset-specific records (e.g. `usgs-nhd`) once actually needed. None of
  these block current work — they block those specific sources becoming
  usable.

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
