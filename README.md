# OrcMaps

An offline-first map engine for embedded devices. ESP32-class hardware
(ESP32-S3, ESP32-P4) is the first target; the core is deliberately platform-
portable so other microcontroller-class devices can adopt it later.

OrcMaps renders useful, readable maps entirely from local storage (SD card) —
no network connection required to display a map. It owns base geography
(roads, water, boundaries, place labels) and a small set of generic drawing
primitives (markers, polylines, polygons). It does **not** know what an
aircraft, a LoRa node, or an RF site is — applications translate their own
domain data into OrcMaps overlays.

[`hardcoreerik/OrcSDR`](https://github.com/hardcoreerik/OrcSDR) is the first
consumer, replacing its prototype `ORCMAP1` implementation. OrcMaps is built
so unrelated ESP32 projects can use it too.

## Status

Early development — architecture and format decision in progress. See
[`docs/ORCMAP1_AUDIT.md`](docs/ORCMAP1_AUDIT.md) for the prototype this
project supersedes, and [`docs/FORMAT_DECISION.md`](docs/FORMAT_DECISION.md)
(once written) for the evidence behind the map-pack format choice.

## Design principles

1. **Offline first.** A downloaded map pack must render with zero network access.
2. **One basemap, many applications.** Base geography is shared; every consumer supplies its own overlays.
3. **Bounded memory.** Map/pack size must never determine RAM usage — everything streams.
4. **Portable core.** No application state (aircraft, nodes, RF data) inside the engine.
5. **Display-backend-agnostic core, M5GFX-first adapter.** Rendering backend and map core are separable.
6. **Map data is not firmware.** Packs install/update independently of the engine and the consuming app.
7. **User data is not map-pack data.** Markers/waypoints survive pack replacement, are never uploaded during pack discovery.
8. **Safe updates.** A corrupt or partial download never replaces a working pack.
9. **Provenance matters.** Every pack carries source, license, attribution, and hash metadata.
10. **No unauthorized tile scraping.** Packs are built from legitimate extract data (e.g. OSM `.pbf` extracts), never bulk-scraped raster tile servers.

## Repository layout

```
include/orcmap/      Public headers — the portable API
src/core/             Geo math, tile addressing, viewport
src/storage/          ByteSource abstraction (no direct filesystem coupling)
src/tiles/            Pack/archive format reader
src/render/           Rendering backend interface + tile→draw-call translation
src/overlays/         Marker/polyline/polygon overlay primitives
src/cache/            Bounded LRU tile caches (RAM/PSRAM + optional SD render cache)
adapters/m5gfx/        M5GFX/LovyanGFX rendering backend
adapters/esp_idf/      ESP-IDF filesystem ByteSource adapter
tools/pack-builder/    Host-side: OSM extract -> map pack
tools/pack-inspect/    Host-side: inspect a pack's metadata/contents
tools/pack-verify/     Host-side: validate a pack against its manifest
examples/m5stack-tab5/ Full example on M5Stack Tab5 (ESP32-P4)
examples/generic-esp32/Minimal ESP32-S3 example
tests/host/            Host-buildable unit tests (no hardware required)
docs/                  Architecture, format decision, porting, licensing docs
```

## License

AGPL-3.0-only, with commercial licensing available by agreement — the same
model as [`hardcoreerik/esp-rtl-sdr`](https://github.com/hardcoreerik/esp-rtl-sdr).
See [`LICENSE`](LICENSE) and [`LICENSING.md`](LICENSING.md).

Map **data** distributed alongside this engine (e.g. prebuilt map packs) is
subject to its own source license (typically OpenStreetMap's ODbL) —
engine licensing and data licensing are independent. See
[`docs/DATA_AND_LICENSING.md`](docs/DATA_AND_LICENSING.md).
