# OrcMaps Current Status

Last updated: 2026-09-13

## Current development focus

Phase 0 (repo foundation, format decision) and Phase 1 (format vertical
slice, synthetic fixture) are complete. Phase 2 (rendering foundation) is
underway.

**IMPLEMENTED:** PMTiles reader, Web Mercator math, schema-agnostic MVT
decode, OrcMaps Feature / Geometry model, MVT→FeatureTile translation,
style system, host + consumer tests, provenance registry,
`examples/generic-esp32` ESP-IDF compile/link of the portable core
(ESP-IDF 6.0.2 / `esp32p4`, no graphics framework).

**EXPERIMENTAL / SKETCH:** `orcmap::experimental::AssignFeatureKinds`
(layer-name heuristic, not the tile schema). `adapters/m5gfx/` (header-only
Color→RGB565 + LovyanGFX helpers). The adapter still takes MVT types —
retarget onto `Feature` is later. Not compiled into core.

**PLANNED (Phase 2 remaining):** graphics-independent render seam,
retarget M5GFX onto Feature, tile-payload decompression seam, minimal
Viewport, host render proof, `adapters/esp_idf` ByteSource.

**BLOCKED on real data (later):** tile-content schema / stable
`FeatureKind` mapping, Lane County pack, performance numbers. Do not
finalize schema from `tiny.mvt`.

## Repository / branch state

- `hardcoreerik/orcmaps`, `main` branch.
- Local working copy: `F:\Ai\OrcMaps`.
- Treat `git log` / `git status` as authoritative over this paragraph.
  MVT decoder, `examples/generic-esp32`, and the `adapters/m5gfx` sketch
  are already on `origin/main` as of `be2b8fb`. This file's job is to
  describe that HEAD honestly, not to recap one session.
- OrcSDR's own branch (`grok/orcmap1-readable-map`, worktree
  `F:\Ai\OrcSDR-Temp\OrcSDR-orcmap1-readable-map`) is **untouched** by this
  work — no integration has started, per `ROADMAP.md` Phase 4 not begun.

## What works today

- `orcmap::PmTilesReader` opens a PMTiles v3 archive, parses its header and
  root directory (gzip-compressed, via vendored miniz), and looks up
  individual tiles by z/x/y via Hilbert tile-ID addressing, including
  leaf-directory indirection and sparse/missing-tile handling — verified
  byte-exact against a fixture built with the official reference
  implementation.
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
- Host test suite: **37 test functions, all passing**.

## What is partially working

- **`orcmap::experimental::AssignFeatureKinds` — EXPERIMENTAL.** Layer-name
  + `class` heuristic for tests. Not the production tile schema. Unrecognized
  layers stay `kind_assigned == false`. Replace after a real Lane County
  tile is measured.
- **`adapters/m5gfx/` — EXPERIMENTAL sketch, not a renderer.** Header-only
  `ToRgb565` and `DrawFeature` against `lgfx::v1::LovyanGFX&`. Not a CMake
  component, not compiled into core, not used by `examples/generic-esp32`,
  no tests. It currently includes `orcmap/mvt.hpp` and draws `MvtFeature`
  geometry — that coupling is a defect of the sketch now that `Feature`
  exists. Retarget onto `Feature`; do not grow the MVT-typed path.
- The PMTiles reader has only been exercised against a synthetic fixture
  whose directory fits in the root (no leaf-directory fetch exercised
  end-to-end against real data) and whose tile payloads are uncompressed
  (gzip inflate is only exercised on the directory, not on tile bytes) —
  see `ROADMAP.md` Phase 1 risks. `GetTile()` returns bytes as stored;
  `DecodeMvtTile()` wants decompressed MVT; there is no public generic
  tile-payload decompression seam yet.
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

Feature / Geometry + MVT translation slice is complete. Next is the
graphics-independent render seam (evaluate RenderTarget vs render-command),
then retarget M5GFX onto `Feature`. Not OrcSDR.

## Current blockers

- No graphics-independent renderer / Viewport, so nothing yet draws a
  `FeatureTile`. The Feature model exists; the render seam does not.
- No public generic tile-payload decompression seam (`GetTile()` returns
  stored bytes; `DecodeMvtTile()` wants decompressed MVT).
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
adapter is an optional sketch.

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

None yet. No renderer core, no real pack, no on-device map draw.
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
(one `ctest` entry, `orcmap_host_tests`, itself running 37 test functions
covering geo math, PMTiles container parsing, the style system, the
runtime attribution API, MVT decode, and Feature/MVT translation — see
`ARCHITECTURE.md` "Testing architecture").

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
real repository; 17 documentation-truth unit tests pass, 20
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

1. Evaluate RenderTarget vs render-command stream; implement the smallest
   graphics-independent seam; add a host-testable render proof.
2. Retarget `adapters/m5gfx` onto `Feature` / that seam; remove
   `orcmap/mvt.hpp` from it.
3. Public generic tile-payload decompression path (`GetTile` → decompress
   → format decoder). Unsupported Brotli/Zstd stay clean failures.
4. Minimal Viewport (center lat/lon, zoom, screen size).
5. `adapters/esp_idf` ByteSource.
6. Then a real Lane County/Eugene pack from the already-`CONFIRMED`
   `openstreetmap` record — schema, leaf directories, compressed tiles,
   and performance numbers come from that evidence, not `tiny.mvt`.

Do not do this tranche: OrcSDR integration, a second graphics framework,
overlay application features, pack marketplace UI.

Ongoing discipline (not a one-time action): keep both
`tools/check_documentation_truth.py` and `tools/check_data_provenance.py`
passing on every change — run them locally before committing, same
workflow as their CI counterparts.

## Files / areas currently in motion

None — Feature / Geometry slice at a stopping point. Renderer work has
not started.

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
