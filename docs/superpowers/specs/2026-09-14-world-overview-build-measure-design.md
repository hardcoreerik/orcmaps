# Natural Earth World Overview Build and Measurement Design

## Goal

Build and measure graphics-adapter-independent OrcMaps world-overview PMTiles
through zooms 6, 7, and 8 from the already acquired Natural Earth 5.1.2
bundle. Use measured artifacts and generic host renders to recommend product
cutoffs without publishing or hardware testing.

## Fixed inputs and tools

- Map inputs are restricted to
  `data/local/world-overview/natural-earth/5.1.2/` and the checked-in
  `tools/pack-builder/world_overview_sources.json` hashes.
- Planetiler is pinned to 0.10.2, commit
  `0e5588c4a6e8c29a270a33afe8df62027d889604`, and runs on Java 21.
- Candidate derivation uses official go-pmtiles v1.28.2 provisioned explicitly
  under ignored local storage. The executable, its archive, and temporary
  files never enter Git and are never runtime dependencies.
- Neither builder performs hidden downloads. Missing tools or inputs cause a
  clear failure.

## Pack profile

Planetiler 0.10.2's YAML configuration cannot set a feature maximum zoom, so
it cannot prevent 110m and 50m geometry from leaking into higher zooms. The
checked-in minimal Java profile is therefore `orcmaps-overview-1`. It reads the
seven selected Natural Earth themes at each of three scales and emits five
generic MVT layers:

| Zoom | Natural Earth scale | Layers |
|---|---|---|
| 0-2 | 110m | land, water, waterway, boundary, place |
| 3-5 | 50m | land, water, waterway, boundary, place |
| 6-8 | 10m | land, water, waterway, boundary, place |

Ocean and lakes share `water`. Admin-0 and admin-1 share `boundary`, with only
the minimum admin-level discriminator. Places retain `name` and `scalerank`.
No road source or Natural Earth attribute table is copied wholesale.

Natural Earth boundary geometry is reproduced as supplied. OrcMaps does not
reinterpret or independently assert geopolitical positions.

## Build and artifact flow

`build_world_overview.py` validates all 21 source archive hashes and required
shapefile components, validates the Planetiler and Java versions, refuses
existing outputs unless `--force` is explicit, and invokes Planetiler without
network flags to compile/run the profile and make `world-overview-z8.pmtiles`.
It then invokes a caller-
supplied, version-verified go-pmtiles v1.28.2 executable to extract z0-z6 and
z0-z7.

Each candidate receives a local manifest and checksum. The manifest records
the profile, bounds, zoom range, immutable identity, Natural Earth provenance
and input hashes, tool versions, exact commands, output size/hash, and optional
non-required courtesy credit. All generated data lives beneath gitignored
`data/local/world-overview/build/`.

## OrcMaps compatibility and classification

A single public compatibility predicate owns the two supported input profile
identifiers: `openmaptiles-3.16` and `orcmaps-overview-1`. Manifest validation
calls it; no registry, factory, or plugin mechanism is introduced.

Profile-specific classification remains outside the generic MVT decoder. The
overview mapping is land to `kLand`, water and waterway to `kWater`, boundary
to `kBoundary`, and place to `kLabelPrimary`. Existing OpenMapTiles behavior
continues unchanged.

## Graphics independence

The build and runtime validation path is:

```
Natural Earth -> Planetiler -> PMTiles/MVT -> FeatureTile -> FeatureKind
              -> MapStyle -> generic RenderTarget
```

Profile files, manifests, builder code, classification, pack identity, and
styles contain no M5GFX, M5Unified, LVGL, board, Tab5, display-controller, or
OrcSDR types. Host validation uses `orcmap::host::FramebufferTarget`. A clean
consumer build with `adapters/m5gfx/` excluded is the executable independence
gate. The resulting PMTiles are not rebuilt per graphics adapter.

## Measurement and visual evidence

For each z6/z7/z8 candidate, record archive size/hash, addressed and non-empty
tile counts, stored tile size distribution, decompressed sizes, representative
feature counts, and host lookup/decompress/decode/translate/classify/render/
total timings. Use existing tooling where possible and add only the smallest
missing host inspection behavior.

Render world, North America, Pacific Northwest, Oregon, and the Springfield
approach in `standard-light` and `orcsdr-dark`. Keep data-content findings
separate from current-renderer visibility, especially that place records may
exist while labels remain invisible.

The final evidence recommends tiny and standard candidates only after comparing
measured usefulness, size, workload, and the handoff point to the existing
regional Springfield pack.

## Safety and stopping conditions

- No OSM planet or other map source is downloaded.
- No OpenMapTiles profile is used for this pack.
- No generated PMTiles, render, tool binary, extracted source, or temporary
  file is committed.
- No hardware is flashed or benchmarked.
- No resolver redesign, Pack Manager, cloud automation, GitHub Release, or
  OrcSDR work is included.
- Work remains on `codex/world-overview-build-measure` for review and stops
  after the complete report. It is not merged automatically.
