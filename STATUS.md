# OrcMaps Current Status

Last updated: 2026-09-13

## Current development focus

Phase 0 (repo foundation, format decision) and Phase 1 (format vertical
slice, synthetic fixture) are complete. Phase 2 (rendering foundation) is
underway: the MVT vector-tile decoder is now implemented and host-tested.
What's left in Phase 2: deciding the tile content schema and mapping
decoded features to it, the ESP-IDF/M5GFX adapters, and the renderer core
itself — none of which can start for real without an ESP-IDF/M5GFX
environment this session doesn't have. Work pulled forward from later
phases earlier this session, per explicit request: the style system, and a
full IP/provenance safety system (data provenance registry + checker,
pack-manifest schema, runtime attribution API, consumer integration test,
`CONTRIBUTING.md`).

## Repository / branch state

- `hardcoreerik/orcmaps`, `main` branch, single contributor session so far.
- Local working copy: `F:\Ai\OrcMaps`.
- Prior work (bootstrap, the Documentation Truth CI, the IP/provenance
  safety system, the ESP-IDF compatibility policy) is committed and pushed
  to `origin/main`. This turn's MVT decoder work is complete locally and
  validated, committed/pushed as part of finishing this work — see git log
  for the actual commit if this line is stale by the time you're reading
  it; don't trust prose over `git log`.
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
  `tools/check_data_provenance.py`. **2 of 9 are `CONFIRMED`/approved for
  official use** (Natural Earth, OpenStreetMap); the rest are
  `REVIEW_REQUIRED` pending primary-source verification or (for Overture)
  per-record license filtering — see each record's `notes` field and
  `ROADMAP.md` "Deferred work" for exactly what's outstanding.
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
- Host test suite: **29 test functions, all passing** (25 from before this
  turn + 4 new MVT decoder tests).

## What is partially working

- Nothing is "partially working" in the sense of flaky/incomplete-but-
  running — everything implemented either fully passes its tests or
  doesn't exist yet. Caveats worth knowing:
  - The PMTiles reader has only been exercised against a synthetic fixture
    whose directory fits in the root (no leaf-directory fetch exercised
    end-to-end against real data) and whose tile payloads are uncompressed
    (gzip inflate is only exercised on the directory, not on tile bytes) —
    see `ROADMAP.md` Phase 1 risks.
  - The MVT decoder has likewise only been exercised against a small
    synthetic fixture (3 layers, 1 feature each) built by a reference
    encoder, not a real-world tile with hundreds of features, deeply
    nested multi-ring polygons, or unusual attribute value combinations —
    a real Lane County tile may exercise code paths the current tests
    don't (same caveat pattern as the PMTiles reader above).
  - The data provenance registry's `official_pack_allowed: true` sources
    (Natural Earth, OpenStreetMap) are approved for use, but no pack
    builder exists yet to actually consume them — the registry is ahead of
    the tooling that will read it, deliberately (schema/direction
    established now, per the original request, without forcing incomplete
    pack-building functionality).

## What is being worked on

Nothing mid-flight — this turn's work (the MVT decoder) is at a clean
stopping point (all tests green, all docs consistent with actual code,
both CI checkers passing against the real repo). Next session should pick
up Phase 2's remaining items (tile content schema decision, ESP-IDF/M5GFX
adapters, renderer core) per `ROADMAP.md`, or resolve one of the
`REVIEW_REQUIRED` provenance records if that's the priority instead.

## Current blockers

- No ESP-IDF toolchain available in this development environment — the
  ESP-IDF component registration (`CMakeLists.txt`) is written to match
  `esp-rtl-sdr`'s proven pattern but has never actually been built against
  a real ESP-IDF SDK. This blocks any on-device verification until an
  ESP-IDF build environment is available.
- No M5GFX/LovyanGFX headers available in this environment either — the
  `adapters/m5gfx` renderer backend can be designed but not compiled here.
- No real OSM extract pipeline exists yet (Geofabrik download + osmium/
  Planetiler) — needed before Phase 3 can start.
- Several data sources are blocked on primary-source verification that
  couldn't be completed from this environment: NOAA ETOPO's ISO metadata
  page returned HTTP 503, USDOT NAD's disclaimer page returned HTTP 403
  (twice, two mirrors) — both need a direct re-fetch (possibly via an
  actual browser) before those records can move from `REVIEW_REQUIRED` to
  `CONFIRMED`.

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

None yet. No renderer, no real pack, no on-device build.
`docs/PERFORMANCE.md` is a placeholder. The only real numbers in the repo are
cited third-party numbers from the yuiseki precedent, in
`docs/FORMAT_DECISION.md` — not OrcMaps' own measurements.

## Test status

Host engine tests:

```
cd F:\Ai\OrcMaps
cmake -S tests/host -B build -G "Visual Studio 18 2026"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Result as of this writing: `100% tests passed, 0 tests failed out of 1`
(one `ctest` entry, `orcmap_host_tests`, itself running 29 test functions
covering geo math, PMTiles container parsing, the style system, the
runtime attribution API, and MVT decode — see `ARCHITECTURE.md` "Testing
architecture").

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
version `0.1.0` but nothing consumes it yet. `idf_component.yml`'s
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
4. miniz (tinfl subset) vendored for inflate, MIT-licensed, recorded in
   `docs/DEPENDENCY_LEDGER.md` — chosen over requiring system zlib so the
   same code path works identically on host and (eventually) ESP-IDF.
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
   deterministic-stdlib-only design). Nine starter records reviewed; only
   2 are `CONFIRMED`/approved today — the checker actively prevents the
   other 7 from being used officially until resolved, which is the system
   working as intended, not a gap.
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

## Next 3-7 actions

1. Decide the tile content schema (`docs/FORMAT_DECISION.md` "Deferred") —
   needs a real Lane County MVT tile to decide from, not further staring
   at the synthetic fixture. Write the `DecodedFeature -> FeatureKind`
   mapping once decided.
2. Build a real (small) OSM-derived `.pmtiles` archive for Lane County
   using a proper extract pipeline (Geofabrik + Planetiler or tippecanoe),
   to replace the synthetic fixture as the thing actually being decoded —
   this can now reference the `openstreetmap` provenance record, which is
   already `CONFIRMED`/approved, and gives the MVT decoder its first
   real-world exercise (see "What is partially working" above).
3. Write `adapters/esp_idf`'s `ByteSource` once an ESP-IDF environment is
   available.
4. Write `adapters/m5gfx`'s Color→RGB565 conversion and draw-call backend.
5. Get a first ESP-IDF build of the `orcmap` component actually compiling
   against a real ESP-IDF SDK (currently unverified).
6. Record real performance numbers in `docs/PERFORMANCE.md` as soon as
   there's anything to measure.
7. Resolve the `REVIEW_REQUIRED` provenance records (see `ROADMAP.md`
   "Deferred work") when there's time between engine-critical-path work —
   none of them block Phase 2/3, since Phase 3's Lane County pack only
   needs the already-`CONFIRMED` `openstreetmap` record.

Ongoing discipline (not a one-time action): keep both
`tools/check_documentation_truth.py` and `tools/check_data_provenance.py`
passing on every change — run them locally before committing, same
workflow as their CI counterparts.

## Files / areas currently in motion

None — clean stopping point. Every file this session touched or added is
either fully implemented and tested, or a deliberate, documented
placeholder (e.g. the `usgs-national-map` registry record, which exists
specifically to record a non-approval rather than to approve too broadly).

## Notes for the next development session

- Read `PROJECT_TRUTH.md` first for durable constraints (including the
  new "IP and Provenance Safety Model" and "Public API and Versioning"
  sections), then this file for what's actually done, then `ROADMAP.md`
  Phase 2 for what's next.
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
