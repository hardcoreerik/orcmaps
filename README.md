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

Early development. The map-pack container format is decided (PMTiles v3 —
see [`docs/FORMAT_DECISION.md`](docs/FORMAT_DECISION.md)). Implemented and
host-tested: PMTiles archive reader, Web Mercator tile math, MVT container
decode (schema-agnostic), and the style system. An ESP-IDF compile/link
smoke test exists at [`examples/generic-esp32`](examples/generic-esp32)
and does **not** require M5GFX. Not yet built: the OrcMaps-owned feature
model, viewport, renderer core, overlays, cache, and a real on-device map
draw. `adapters/m5gfx` is an experimental sketch, not a finished
integration. See [`STATUS.md`](STATUS.md) for the live snapshot and
[`docs/ORCMAP1_AUDIT.md`](docs/ORCMAP1_AUDIT.md) for the OrcSDR prototype
this project supersedes.

## Design principles

1. **Offline first.** A downloaded map pack must render with zero network access.
2. **One basemap, many applications.** Base geography is shared; every consumer supplies its own overlays.
3. **Bounded memory.** Map/pack size must never determine RAM usage — everything streams.
4. **Portable core.** No application state (aircraft, nodes, RF data) inside the engine.
5. **Graphics-framework-independent core.** M5GFX is the first *reference integration* because OrcSDR/Tab5 is the first real consumer — it is not the renderer architecture.
6. **Map data is not firmware.** Packs install/update independently of the engine and the consuming app.
7. **User data is not map-pack data.** Markers/waypoints survive pack replacement, are never uploaded during pack discovery.
8. **Safe updates.** A corrupt or partial download never replaces a working pack.
9. **Provenance matters.** Every pack carries source, license, attribution, and hash metadata — enforced by a machine-readable registry (`data/sources/`) and CI, not just documented. See [`docs/DATA_AND_LICENSING.md`](docs/DATA_AND_LICENSING.md).
10. **No unauthorized tile scraping.** Packs are built from legitimate extract data (e.g. OSM `.pbf` extracts), never bulk-scraped raster tile servers.

## Repository layout

```
include/orcmap/        Public headers — the portable API
src/core/              Geo math, tile addressing (viewport not yet implemented)
src/storage/           Reserved; ByteSource interface lives in include/orcmap/
src/tiles/             PMTiles reader + MVT decoder
src/render/            Style system today; renderer core not yet implemented
src/overlays/          Planned marker/polyline/polygon overlay primitives
src/cache/             Planned bounded LRU tile caches
adapters/host/         stdio ByteSource for host tests/tools
adapters/m5gfx/        EXPERIMENTAL M5GFX/LovyanGFX sketch (not compiled into core)
adapters/esp_idf/      Planned ESP-IDF filesystem ByteSource adapter
tools/pack-builder/    Planned host-side: OSM extract -> map pack
tools/pack-inspect/    Planned host-side pack inspector
tools/pack-verify/     Planned host-side pack verifier
examples/m5stack-tab5/ Planned Tab5 graphics example
examples/generic-esp32/ESP-IDF compile smoke test (no graphics framework)
tests/host/            Host-buildable unit tests (no hardware required)
tests/consumer/        External-consumer build gate (public headers only)
data/sources/          Data provenance registry
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

## Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md), including how licensing and data
provenance apply to contributions.
