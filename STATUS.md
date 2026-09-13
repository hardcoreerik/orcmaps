# OrcMaps Current Status

Last updated: 2026-09-13

## Current development focus

Just finished Phase 0 (repo foundation, format decision) and most of the
"do this ahead of schedule because it was explicitly requested" style
system work. Phase 1 (format vertical slice) is done against a synthetic
fixture. Phase 2's critical path — an MVT vector-tile decoder and a first
real renderer — has not been started yet.

## Repository / branch state

- `hardcoreerik/orcmaps`, `main` branch, single contributor session so far.
- Local working copy: `F:\Ai\OrcMaps`.
- Not yet committed to git as of this writing (working tree has all files
  described below; commit + push is the immediate next action).
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
- Host test suite: **17 test functions, all passing.**

## What is partially working

- Nothing is "partially working" in the sense of flaky/incomplete-but-
  running — everything implemented either fully passes its tests or
  doesn't exist yet. The one caveat: the PMTiles reader has only been
  exercised against a synthetic fixture whose directory fits in the root
  (no leaf-directory fetch exercised end-to-end against real data) and
  whose tile payloads are uncompressed (gzip inflate is only exercised on
  the directory, not on tile bytes) — see `ROADMAP.md` Phase 1 risks.

## What is being worked on

Nothing mid-flight — this session's work is at a clean stopping point
(all tests green, all docs consistent with actual code). Next session
should start at Phase 2 (MVT decoder) per `ROADMAP.md`.

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

## Known bugs

None currently — the one found (unsigned-integer underflow in
`LatLonToTile` when a point clamps to exactly the Mercator latitude limit,
causing `std::min` on wrapped-around unsigned values to pick the wrong
tile row) was caught by `tests/host/test_geo.cpp` and fixed in
`src/core/geo.cpp` (clamp in floating point before casting to `uint32_t`)
during this same session. Documented in `ARCHITECTURE.md` "Projection /
coordinates" as a cautionary note for anyone touching that function again.

## Current performance measurements

None yet. No renderer, no real pack, no on-device build. `docs/
PERFORMANCE.md` is a placeholder. The only real numbers in the repo are
cited third-party numbers from the yuiseki precedent, in
`docs/FORMAT_DECISION.md` — not OrcMaps' own measurements.

## Test status

```
cd F:\Ai\OrcMaps
cmake -S tests/host -B build -G "Visual Studio 18 2026"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Result as of this writing: `100% tests passed, 0 tests failed out of 1`
(one `ctest` entry, `orcmap_host_tests`, itself running 17 test functions
covering geo math, PMTiles container parsing, and the style system — see
`ARCHITECTURE.md` "Testing architecture" for the full list).

## Integration status

Not integrated with OrcSDR. `idf_component.yml` exists and declares
version `0.1.0` but nothing consumes it yet.

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
5. Style system built now (ahead of the original phase ordering) because
   it was explicitly requested mid-session; `MapStyle`/`ResolveFeatureStyle`
   API is considered stable enough for a renderer to build against.

## Next 3-7 actions

1. Commit and push this work to `hardcoreerik/orcmaps` `main`.
2. Start Phase 2: pick/build a minimal MVT (vector tile / protobuf)
   decoder — evaluate hand-rolled minimal parser vs. an existing small
   library before choosing, per `docs/DEPENDENCY_LEDGER.md`'s "candidates
   under evaluation" entry.
3. Build a real (small) OSM-derived `.pmtiles` archive for Lane County
   using a proper extract pipeline (Geofabrik + Planetiler or tippecanoe),
   to replace the synthetic fixture as the thing actually being decoded.
4. Write `adapters/esp_idf`'s `ByteSource` once an ESP-IDF environment is
   available.
5. Write `adapters/m5gfx`'s Color→RGB565 conversion and draw-call backend.
6. Get a first ESP-IDF build of the `orcmap` component actually compiling
   against a real ESP-IDF SDK (currently unverified).
7. Record real performance numbers in `docs/PERFORMANCE.md` as soon as
   there's anything to measure.

## Files / areas currently in motion

None — clean stopping point. All files listed in `ARCHITECTURE.md`'s
component table are either fully implemented+tested or empty placeholder
directories with no half-written code in them.

## Notes for the next development session

- Read `PROJECT_TRUTH.md` first for durable constraints, then this file
  for what's actually done, then `ROADMAP.md` Phase 2 for what's next.
- The synthetic test fixture (`tests/fixtures/tiny.pmtiles`) is
  deliberately not real map data — don't try to "improve" it into a real
  map; build a real archive separately for Phase 3, and keep the synthetic
  fixture as the fast, deterministic container-format regression test it's
  designed to be.
- Before adding any new third-party dependency (an MVT decoder is the
  obvious next candidate), update `docs/DEPENDENCY_LEDGER.md` first, per
  `LICENSING.md`'s policy — this was followed for miniz and should be
  followed again.
- Don't re-litigate the PMTiles-vs-MBTiles-vs-custom-format decision
  without new evidence — see `PROJECT_TRUTH.md` "Current Technology
  Direction".
