# OrcMaps Current Status

Last updated: 2026-09-13

## Current development focus

Phase 0 (repo foundation, format decision) and Phase 1 (format vertical
slice, synthetic fixture) are complete. Phase 2 (rendering foundation) is
underway.

**IMPLEMENTED:** PMTiles reader, Web Mercator math, schema-agnostic MVT
decode, FeatureKind in `feature_kind.hpp`, Feature / Geometry model,
MVT→FeatureTile translation, style system, immediate `RenderTarget`,
`RenderFeatureTile`, bounded `DecompressPayload` (None/Gzip), host
framebuffer, M5GFX `DisplayTarget` (exported `orcmap/m5gfx/` headers),
host + consumer tests, provenance registry,
`examples/generic-esp32` (no graphics framework), `examples/m5gfx`
compile proof.

**PARTIAL:** Viewport (prepared TileScreenMap; no overzoom, no
antimeridian wrap, no visible-tile enumerator). Zoom 0..31.

**EXPERIMENTAL:** `orcmap::experimental::AssignFeatureKinds`.

**PLANNED (Phase 2 remaining):** `adapters/esp_idf` ByteSource, polygon
holes, line width, overzoom, real Lane County pack. Brotli/zstd tile
compression stay unsupported.

**BLOCKED on real data (later):** tile-content schema / stable
`FeatureKind` mapping, Lane County pack, performance numbers. Do not
finalize schema from `tiny.mvt`.

## Repository / branch state

- `hardcoreerik/orcmaps`, `main` branch.
- Local working copy: `F:\Ai\OrcMaps`.
- Treat `git log` / `git status` as authoritative over this paragraph.
  MVT decoder, `examples/generic-esp32`, and `adapters/m5gfx` are already
  on `origin/main`. This file's job is to describe HEAD honestly, not to
  recap one session.
- OrcSDR's own branch (`grok/orcmap1-readable-map`, worktree
  `F:\Ai\OrcSDR-Temp\OrcSDR-orcmap1-readable-map`) is **untouched** by this
  work — no integration has started, per `ROADMAP.md` Phase 4 not begun.

## What works today

- `orcmap::PmTilesReader` opens a PMTiles v3 archive, parses its header and
  root directory (via `DecompressPayload`), and looks up individual tiles
  by z/x/y via Hilbert tile-ID addressing, including leaf-directory
  indirection and sparse/missing-tile handling — verified byte-exact
  against a fixture built with the official reference implementation.
  `GetTile()` returns archive bytes as stored. `DecompressPayload()`
  (`include/orcmap/compression.hpp`) inflates kNone/kGzip with a caller
  output budget; kBrotli/kZstd/kUnknown fail. Directories and metadata
  use the same helper. Host-tested through a gzip-compressed MVT fixture
  all the way to host framebuffer pixels.
- `orcmap::LatLonToTile`/`TileToLatLon`/etc. — standard Web Mercator tile
  math, antimeridian-safe, Mercator-latitude-clamp-safe.
- `orcmap::MapStyle` + 4 built-in styles (`orcsdr-dark`, `standard-light`,
  `high-contrast-field`, `night-red-safe`) + `ResolveFeatureStyle()` +
  `StyleManager` runtime switching + `RenderedTileCacheKey`.
- `orcmap::AttributionInfo` / `orcmap::MapSourceInfo` /
  `CollectRequiredAttribution()` — the runtime attribution API shape
  (`include/orcmap/attribution.hpp`, `include/orcmap/map_source.hpp`),
  host-tested. Nothing populates a real `MapSourceInfo` from an opened
  pack yet (no pack manifest/discovery implementation exists) — this is
  the API a future `MapEngine` will feed, not a complete feature.
- Data provenance registry: 9 reviewed datasets under `data/sources/*.json`
  (Natural Earth, OpenStreetMap, U.S. Census TIGER/Line, NOAA ETOPO, USGS
  National Map (deliberate non-approval placeholder), USDOT NAD, Overture
  Places, geoBoundaries gbOpen, Google Open Buildings), validated by
  `tools/check_data_provenance.py`. **4 of 9 are `CONFIRMED`/approved for
  official use** (Natural Earth, OpenStreetMap, geoBoundaries gbOpen,
  Google Open Buildings); the other 5 are `REVIEW_REQUIRED` pending
  primary-source verification or (for Overture) per-record license
  filtering — see each record's `notes` field and `ROADMAP.md` "Deferred
  work" for exactly what's outstanding.
- `tests/consumer/` — a second, independent CMake project proving a
  public-headers-only build works (structurally, via CMake include-path
  visibility, not just as a documented rule).
- Documentation Truth CI (`tools/check_documentation_truth.py`) and its
  sibling, Data Provenance Truth CI (`tools/check_data_provenance.py`) —
  both passing against this repo, both with their own unit test suites.
- `orcmap::DecodeMvtTile()` (`include/orcmap/mvt.hpp`,
  `src/tiles/mvt_decoder.cpp`) — decodes real MVT vector-tile bytes
  (layers, features, geometry, typed attributes) via a hand-rolled minimal
  protobuf reader, no protobuf library dependency. Schema-agnostic by
  design: it does not yet know how to turn a decoded feature into an
  `orcmap::FeatureKind` — that mapping is still blocked on the tile
  content schema decision (`docs/FORMAT_DECISION.md` "Deferred").
  Host-tested against a real MVT fixture built with the `mapbox-vector-tile`
  reference encoder.
- `examples/generic-esp32` — ESP-IDF compile/link smoke test of the
  portable core (geo math + style resolve logged at runtime). Built
  against ESP-IDF 6.0.2, target `esp32p4`. **Does not depend on M5GFX or
  M5Unified.** Does not open a pack, decode MVT at runtime, or draw
  pixels. Not a CI gate.
- `orcmap::Feature` / `Geometry` / `FeatureTile` (`include/orcmap/feature.hpp`)
  — format-independent geometry + properties. `TranslateMvtToFeatureTile()`
  deep-copies MVT into owned Feature data. Does not assign `FeatureKind`.
  Geometry type lives only on `geometry.type`. `FeatureKind` lives in
  `feature_kind.hpp` (Feature does not include style.hpp).
- `RenderFeatureTile` + `RenderTarget` + `orcmap::host::FramebufferTarget`
  — FeatureTile → ResolveFeatureStyle → Viewport → pixels, no M5GFX.
  Unclassified features skipped. First polygon path only.
- Host test suite: **78 test functions, all passing**.

## What is partially working

- **`orcmap::experimental::AssignFeatureKinds` — EXPERIMENTAL.** Layer-name
  + `class` heuristic for tests. Not the production tile schema. Unrecognized
  layers stay `kind_assigned == false`. Replace after a real Lane County
  tile is measured.
- **`adapters/m5gfx/` — optional exported DisplayTarget.** Header-only
  `orcmap/m5gfx/display_target.hpp` implements `RenderTarget` over
  `lgfx::v1::LovyanGFX&`. Core `REQUIRES ""`; consumers that want the
  adapter supply their own M5GFX. No MVT types, no map semantics. Opaque
  RGB565 (`Color.a` ignored). `examples/generic-esp32` still does not
  depend on M5GFX/M5Unified. Compile proof only — no on-device visual
  verification in this repo.
- The PMTiles reader has only been exercised against synthetic fixtures
  whose directories fit in the root (no leaf-directory fetch exercised
  end-to-end against real data). Tile gzip is proven on
  `tests/fixtures/tiny-gzip.pmtiles`, not on a real OSM extract.
- The MVT decoder has likewise only been exercised against a small
  synthetic fixture (3 layers, 1 feature each) built by a reference
  encoder, not a real-world tile with hundreds of features, deeply
  nested multi-ring polygons, or unusual attribute value combinations —
  a real Lane County tile may exercise code paths the current tests
  don't.
- The data provenance registry's `official_pack_allowed: true` sources
  (the 4 `CONFIRMED` records) are approved for use, but no pack builder
  exists yet to actually consume them.

## What is being worked on

Nothing in this slice. Next: a real OSM-derived Springfield / Lane County
(97477) pack through PMTiles → OrcMaps → 1280×720 host render. Not OrcSDR.
Do not freeze the tile schema from synthetic fixtures.

## Current blockers

- M5GFX `DisplayTarget` exists; there is no on-device visual verification
  in this repo yet (compile proof + host pixels only).
- Brotli/zstd tile compression is unsupported (`DecompressPayload` fails).
- Viewport is PARTIAL (no overzoom, no antimeridian wrap, no visible-tile
  set). Polygon holes are not subtracted. Line `width_px` is not
  rasterized.
- No real OSM extract pipeline exists yet (Geofabrik download + osmium/
  Planetiler) — needed before Phase 3 can start, and before the tile
  content schema can be decided from evidence.
- Several data sources are blocked on primary-source verification: NOAA
  ETOPO's ISO metadata page returned HTTP 503, USDOT NAD's disclaimer page
  returned HTTP 403 (twice, two mirrors) — both need a direct re-fetch
  before those records can move from `REVIEW_REQUIRED` to `CONFIRMED`.
  These do **not** block Phase 2/3 (Lane County can use the already-
  `CONFIRMED` `openstreetmap` record).

No longer blockers (corrected 2026-09-13): ESP-IDF 6.0.2 is present and
`examples/generic-esp32` has compiled/linked the portable core for
`esp32p4`. M5GFX headers are not required for core work; the m5gfx
adapter is an optional exported include surface.

## Known bugs

None currently — the one found (unsigned-integer underflow in
`LatLonToTile` when a point clamps to exactly the Mercator latitude limit,
causing `std::min` on wrapped-around unsigned values to pick the wrong
tile row) was caught by `tests/host/test_geo.cpp` and fixed in
`src/core/geo.cpp` (clamp in floating point before casting to `uint32_t`)
in an earlier part of this same session. Documented in `ARCHITECTURE.md`
"Projection / coordinates" as a cautionary note for anyone touching that
function again.

## Current performance measurements

None yet. A host renderer exists, but there is no real map pack and no
on-device map rendering measurement. Host framebuffer tests are
correctness checks, not embedded performance numbers.
`docs/PERFORMANCE.md` is a placeholder. The generic-esp32 smoke test
proves the component *links*; it is not a performance result. The only
real numbers in the repo are cited third-party numbers from the yuiseki
precedent, in `docs/FORMAT_DECISION.md` — not OrcMaps' own measurements.

## Test status

Host engine tests:

```
cd F:\Ai\OrcMaps
cmake -S tests/host -B build -G "Visual Studio 18 2026"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Result as of this writing: `100% tests passed, 0 tests failed out of 1`
(one `ctest` entry, `orcmap_host_tests`, itself running 78 test functions
covering geo math, PMTiles, style, attribution, MVT, Feature/MVT
translation, Viewport, clip, host render, and compression — see
`ARCHITECTURE.md`
"Testing architecture").

Consumer smoke test (separate CMake project):

```
cmake -S tests/consumer -B build-consumer -G "Visual Studio 18 2026"
cmake --build build-consumer --config Release
ctest --test-dir build-consumer -C Release --output-on-failure
```

Result: `100% tests passed, 0 tests failed out of 1`.

Documentation and provenance checkers (from repo root, no CMake needed):

```
python tools/check_documentation_truth.py
python -m unittest discover -s tests -p 'test_documentation_truth.py'
python tools/check_data_provenance.py
python -m unittest discover -s tests -p 'test_data_provenance.py'
```

Results as of this writing: both checkers report `0 errors` against the
real repository; 19 documentation-truth unit tests pass, 20
data-provenance unit tests pass.

## Integration status

Not integrated with OrcSDR. `idf_component.yml` exists and declares
version `0.1.0`. The in-tree `examples/generic-esp32` consumes the
component via `EXTRA_COMPONENT_DIRS` (local convenience, not the
version-pinned `git:` + SHA pattern OrcSDR will use). `idf_component.yml`'s
declared ESP-IDF floor is `>=5.0` — deliberately broad, so it doesn't
conflict with OrcSDR's own `>=5.5.0,<5.6.0` pin. OrcMaps is developed and
tested against the latest ESP-IDF 6.x (currently v6.1) as its own
toolchain target, which is a separate thing from that declared floor —
see `PROJECT_TRUTH.md` "Compatibility Goals" for why the two are kept
distinct. No ESP-IDF version conflict currently blocks Phase 4. The intended
pin pattern (OrcSDR's `git:` URL + full commit-SHA, matching its existing
`esp_rtl_sdr` dependency) is documented in `PROJECT_TRUTH.md` "Public API
and Versioning" but not yet used anywhere, since there's no OrcSDR-side
manifest entry to point at OrcMaps yet.

## Most recent important decisions

1. Container format: PMTiles v3, decided with citations
   (`docs/FORMAT_DECISION.md`).
2. Repo/product naming: repo `hardcoreerik/orcmaps`, product "OrcMaps",
   component/namespace `orcmap`, pack extension `.pmtiles` (not a custom
   extension).
3. CMake layout copies `esp-rtl-sdr`'s exact convention (root
   `CMakeLists.txt` = ESP-IDF component, `tests/host/CMakeLists.txt` =
   separate standalone host project) rather than inventing a new layout.
4. miniz 3.1.2 (`77d0dce8627735138c51770d1799a1ef48f2117d`) vendored as
   the full source snapshot under `third_party/miniz`; only inflate is
   compiled. MIT-licensed, recorded in `docs/DEPENDENCY_LEDGER.md`.
5. Style system built ahead of the original phase ordering, per explicit
   request; `MapStyle`/`ResolveFeatureStyle` API is considered stable
   enough for a renderer to build against.
6. Documentation Truth CI added, adapted from OrcSDR's own. Running it
   against this repo for the first time found and fixed real drift (a
   stale test-count claim, misplaced doc path references).
7. Local worktree convention set: `F:\Ai\OrcMaps-Temp\`, matching OrcSDR's
   `F:\Ai\OrcSDR-Temp\` pattern.
8. **IP/provenance safety model established as a durable `PROJECT_TRUTH.md`
   principle**, with a real enforcement mechanism: an 11-value license-class
   enum, three map-pack policy classes (Clean/Permissive/Open), a JSON
   provenance registry (`data/sources/`), and Data Provenance Truth CI
   (`tools/check_data_provenance.py`, mirroring Documentation Truth's
   deterministic-stdlib-only design). Nine starter records reviewed; 4 are
   `CONFIRMED`/approved today — the checker actively prevents the other 5
   from being used officially until resolved, which is the system working
   as intended, not a gap.
9. Code-vs-data license separation formalized architecturally, not just in
   prose: `orcmap::AttributionInfo`/`MapSourceInfo` establish the runtime
   API shape without forcing a full `MapEngine` implementation that
   doesn't exist yet.
10. Public API boundary made structurally enforced (`tests/consumer/`),
    not just documented — a build failure now results if consumer code
    reaches into `src/`/`third_party/` internals.
11. `CONTRIBUTING.md` added with a DCO-style (Developer Certificate of
    Origin) contribution certification, explicitly flagging that a formal
    CLA + attorney review is the right next step if contribution volume
    ever outgrows this lightweight approach — not adopted preemptively.
12. ESP-IDF compatibility: `idf_component.yml` floor stays broad
    (`>=5.0`, satisfied by OrcSDR's current `>=5.5.0,<5.6.0` pin); the
    latest ESP-IDF 6.x (v6.1) is OrcMaps' own dev/CI toolchain target, a
    deliberately separate concept — see `PROJECT_TRUTH.md` "Compatibility
    Goals". Confirmed with the user: OrcSDR is expected to eventually port
    to the latest ESP-IDF itself; this floor is not expected to be raised
    to chase our own dev toolchain.
13. MVT decoder: hand-rolled minimal protobuf reader
    (`orcmap::DecodeMvtTile()`), not a library — a general-purpose
    protobuf parser was judged not worth the footprint for four fixed
    message shapes. Schema-agnostic by design; recorded in
    `docs/DEPENDENCY_LEDGER.md` "Resolved: MVT decoding".
14. Core graphics independence: M5GFX is the first *reference
    integration*, not the renderer architecture. `examples/generic-esp32`
    must not depend on M5GFX/M5Unified. If `adapters/m5gfx/` were deleted,
    core must still build and host-test.
15. GitHub Actions today are Documentation Truth + Data Provenance Truth
    only. Host C++ tests, consumer smoke test, ESP-IDF compile, sanitizers,
    and fuzzing are **not** CI gates yet.

## Next 3-7 actions

1. Real OSM-derived Springfield / Lane County (97477) geography as a
   PMTiles archive, rendered 1280×720 on the host. Schema, leaf
   directories, and performance numbers come from that evidence, not
   `tiny.mvt`.
2. `adapters/esp_idf` ByteSource (needed before on-device pack open).

Do not do this tranche: OrcSDR integration, a second graphics framework,
overlay application features, pack marketplace UI.

Ongoing discipline (not a one-time action): keep both
`tools/check_documentation_truth.py` and `tools/check_data_provenance.py`
passing on every change — run them locally before committing, same
workflow as their CI counterparts.

## Files / areas currently in motion

None — adapter packaging + bounded gzip decompression at a stopping
point. OrcSDR not started.

## Notes for the next development session

- Read `PROJECT_TRUTH.md` first for durable constraints, then this file
  for what's actually done, then `ROADMAP.md` Phase 2 for what's next.
  If those three disagree with the code, the code wins — then fix the
  docs in the same change.
- The synthetic test fixture (`tests/fixtures/tiny.pmtiles`) is
  deliberately not real map data — don't try to "improve" it into a real
  map; build a real archive separately for Phase 3, and keep the synthetic
  fixture as the fast, deterministic container-format regression test it's
  designed to be.
- Before adding any new third-party **code** dependency, update
  `docs/DEPENDENCY_LEDGER.md` first. Before using any new **data** source,
  add a `data/sources/*.json` record first and make sure
  `tools/check_data_provenance.py` passes — see `CONTRIBUTING.md`.
- Don't re-litigate the PMTiles-vs-MBTiles-vs-custom-format decision
  without new evidence — see `PROJECT_TRUTH.md` "Current Technology
  Direction". Same for the provenance/licensing model — see "IP and
  Provenance Safety Model" in the same file.
- A `REVIEW_REQUIRED` provenance record is not a bug to quietly "fix" by
  flipping its confidence to `CONFIRMED` without actually re-verifying the
  primary source. That defeats the entire point of the system.
