# Data and licensing

This document is the map-**data** counterpart to [`../LICENSING.md`](../LICENSING.md)
(which covers engine code). Read both — they are deliberately independent.

## Rule: no bulk tile scraping

OrcMaps map packs must never be built by bulk-downloading a raster tile
service such as `tile.openstreetmap.org`. OSM's public tile service explicitly
prohibits using bulk tile downloads to construct offline archives for
redistribution. Packs must be built from legitimate extract data intended for
downstream processing:

- Raw OpenStreetMap `.osm.pbf` extracts (e.g. Geofabrik regional extracts, or
  the full OSM planet file) processed with a proper extraction/tiling
  pipeline (osmium, Planetiler, tippecanoe, or equivalent) — this is the
  default source.
- Any other data source whose terms explicitly permit offline redistribution,
  evaluated and documented per-source before use.

## Required pack provenance metadata

Every generated map pack must carry (in its manifest, not baked into
geometry):

- `source` — human-readable name (e.g. "OpenStreetMap")
- `source_url` — reference URL for the source data
- `source_date` — date the source extract was taken
- `license` — the data's license (e.g. "ODbL-1.0")
- `attribution` — required attribution string, must be shown in the UI
  wherever the map is displayed
- `generator` / `generator_version` — the pack-builder tool and version that
  produced the pack
- `bounds`, `min_zoom`, `max_zoom`
- `created_at` — pack build timestamp
- `data_version` — a version identifier distinct from the pack-format version
- `sha256` — of the pack archive

## OpenStreetMap / ODbL specifics

OSM data is ODbL-licensed. Redistributing OSM-derived map packs requires:

- Visible attribution ("© OpenStreetMap contributors" or equivalent) in any
  UI that displays the map — this must live in the **application's** style/
  UI layer, not be hardcoded into the renderer the way ORCMAP1 baked
  `"OSM contributors"` into `offline_map.cpp`'s draw call. OrcMaps exposes
  attribution as pack metadata; it is the renderer/app's job to display it.
- Share-alike obligations apply to the **derived database** (the map pack),
  not to the engine code that reads it — this is exactly the engine/data
  license separation this document exists to keep clear.

## Engine license vs. data license — do not conflate

- Engine code: AGPL-3.0 / commercial (see [`../LICENSING.md`](../LICENSING.md)).
- Map data: whatever its source dictates (typically ODbL for OSM-derived
  packs). A commercial engine license does **not** grant different rights
  over ODbL data, and AGPL engine code does not force map data into any
  particular license — they are reviewed and tracked independently.

## Per-pack ledger

Following the same pattern as OrcSDR's `docs/DATA_SOURCE_LEDGER.md`, every
published map pack should have a ledger entry recording: publisher, retrieval
method, retrieval timestamp, license/terms review, exact transformation
command + tool version used to build it, SHA-256, and a removal/takedown
contact. This ledger is created alongside the first real published pack
(tracked as part of the Lane County vertical slice) — not yet populated here
since no pack has been built through the new pipeline yet.
