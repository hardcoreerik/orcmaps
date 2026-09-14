# Springfield / 97477 pack builder (host-only)

Reproducible OSM → PMTiles path for the first real-geography host preview.
This is **pack production**, not an OrcMaps runtime dependency. Planetiler
is not vendored and is not linked into the engine.

Do not treat the resulting layer schema as the OrcMaps tile-content
decision. Inspect tiles with `tools/pack-inspect` first.

## Geographic test area

Springfield, Oregon / ZIP 97477 urban bbox (WGS84, west/south/east/north):

```
-123.055, 44.030, -122.960, 44.090
```

Center used for the 1280×720 preview (not hardcoded in core):

```
44.0500 N, 123.0220 W
```

This bbox is test geography. It is not an application concept.

## Source (provenance)

| Field | Value |
|---|---|
| Provider | Geofabrik OSM extract |
| URL | https://download.geofabrik.de/north-america/us/oregon-latest.osm.pbf |
| Format | `.osm.pbf` (OSM Protocolbuffer Binary) |
| License | ODbL 1.0, © OpenStreetMap contributors |
| Provenance record | `data/sources/openstreetmap.json` (`CONFIRMED`) |
| Acquisition | raw extract; **not** `tile.openstreetmap.org` raster tiles |

Record the download date and SHA-256 in `data/local/SOURCE.txt` when you
run the script.

## Toolchain

| Tool | Role | Notes |
|---|---|---|
| Planetiler 0.10.2 | OSM PBF → PMTiles v3 / MVT | `planetiler.jar` (OpenMapTiles profile 3.16.0). Requires Java 21. Build-tool only. |
| OrcMaps `pack-inspect` | inspect / preview | Uses the engine; not a tiler. |

Exact versions are printed by `build_springfield_pack.ps1`.

## Output contract

```
PMTiles v3
TileType = MVT
tile_compression = gzip
maxzoom = 15
```

Brotli/zstd are not used. OrcMaps does not implement those codecs.

## Commands

From the repository root (PowerShell, Temurin JDK 21):

```
pwsh tools/pack-builder/build_springfield_pack.ps1
```

Artifacts land in `data/local/` (gitignored):

```
data/local/oregon-latest.osm.pbf
data/local/springfield-97477.pmtiles
data/local/SOURCE.txt
```

Then:

```
cmake -S tools/pack-inspect -B build-pack-inspect -G "Visual Studio 18 2026"
cmake --build build-pack-inspect --config Release
build-pack-inspect/Release/orcmap_pack_inspect.exe header data/local/springfield-97477.pmtiles
build-pack-inspect/Release/orcmap_pack_inspect.exe preview data/local/springfield-97477.pmtiles --out data/local/springfield-97477-orcsdr-dark.ppm
```
