# Attribution — Springfield / 97477 demo pack

This directory contains **map data**, not OrcMaps engine code.

OrcMaps source is AGPL-3.0-only (see `LICENSING.md`). This PMTiles
archive is a **Derivative Database** of OpenStreetMap and remains under
the Open Database License. Engine license and data license are independent
(`docs/DATA_AND_LICENSING.md`).

## Required attribution

**Both credits are required. Neither replaces the other** — they cover
different layers: the tile-production schema, and the underlying database.

© OpenMapTiles  
https://openmaptiles.org/

© OpenStreetMap contributors  
https://www.openstreetmap.org/copyright

| Layer | License | Obligation | Provenance record |
|---|---|---|---|
| Tile schema (OpenMapTiles 3.16) | **CC BY 4.0** grant from the OpenMapTiles team | visible credit; **no** share-alike | `data/sources/openmaptiles.json` (`CONFIRMED`, reviewed 2026-09-14) |
| Map data (OpenStreetMap) | **ODbL 1.0** | visible credit **and** share-alike | `data/sources/openstreetmap.json` (`CONFIRMED`, reviewed 2026-09-13) |

Both records are `official_pack_allowed: true`.

ODbL share-alike applies to this pack (queryable geometry and attributes).
Neither license changes the OrcMaps engine license.

Planetiler states the OpenMapTiles obligation in its own build output:
"Such tiles are reusable under CC-BY license granted by OpenMapTiles team"
and "Maps made with these vector tiles must display a visible credit".

> **Defect history.** Until 2026-09-14 this pack's manifest recorded only the
> OpenStreetMap credit, so the device displayed an incomplete attribution, and
> the text was additionally double-encoded (`Â©`). Both are fixed, and
> `tools/check_data_provenance.py` now fails any `openmaptiles-*` manifest
> that omits either credit or carries mangled encoding.

## Source extract

| Field | Value |
|---|---|
| Provider | Geofabrik |
| URL | https://download.geofabrik.de/north-america/us/oregon-latest.osm.pbf |
| Download date (UTC) | 2026-09-14 |
| Format | OSM PBF |
| Extract SHA-256 | `5511e363f0cfdc41ac3d6a9b34668b6a34a2131ae88cc638c7f8d818190c6ddc` |
| Extract size | 253,592,262 bytes |

No raster tiles. Not `tile.openstreetmap.org`. Not a commercial tile dump.

## Processing (not a runtime dependency)

| Tool | Version |
|---|---|
| Planetiler | 0.10.2 (`0e5588c4`) |
| Profile | OpenMapTiles 3.16.0 |
| Java | Temurin 21.0.12.1 |
| Tile type | MVT |
| Tile compression | gzip |
| Bounds (W,S,E,N) | -123.055, 44.030, -122.960, 44.090 |

Regeneration (advanced): `tools/pack-builder/build_springfield_pack.ps1`.
Not required for the Tab5 demo.
