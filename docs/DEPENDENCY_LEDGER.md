# Dependency ledger

Every third-party dependency incorporated into OrcMaps (code, not map data —
see [`DATA_AND_LICENSING.md`](DATA_AND_LICENSING.md) for data provenance) is
recorded here before it is added, per [`../LICENSING.md`](../LICENSING.md).

| Name | Version/commit | License | Used for | Code copied? |
|---|---|---|---|---|
| miniz | 3.1.2 (`richgel999/miniz` tag `3.1.2`, commit `77d0dce8627735138c51770d1799a1ef48f2117d`). The vendored `miniz.h` / `miniz.c` self-identify as 3.1.2. | MIT | Inflate gzip/deflate-compressed PMTiles directories (and tile payloads, when a pack uses tile-level gzip) — bounded-memory, no system zlib, same code path on host and ESP-IDF | Yes — vendored under `third_party/miniz/` as the **full 3.1.2 source snapshot** (`miniz.c/.h`, `miniz_common.h`, `miniz_export.h`, `miniz_tinfl.c/.h`, `miniz_tdef.c/.h`, `miniz_zip.c/.h`, plus `LICENSE`). **Compiled** sources are only `miniz.c` and `miniz_tinfl.c`, with `MINIZ_NO_DEFLATE_APIS` / `MINIZ_NO_ZLIB_APIS` / `MINIZ_NO_ARCHIVE_APIS` so deflate/zip APIs are compiled out of the object files. The extra tdef/zip sources remain in the snapshot because `miniz.h` includes those headers; they are not linked. Full license text at `third_party/miniz/LICENSE`. Do not re-vendor from `master` or an unknown snapshot. |
| `pmtiles` Python package (protomaps) | 3.7.0, BSD-3-Clause | Build tool only, used by `tests/fixtures/generate_fixture.py` to produce the committed test fixture via the official reference implementation | No — not vendored, not a runtime or build dependency of OrcMaps itself; a dev-only tool invoked to generate one committed binary fixture. Not part of the shipped product. |
| `mapbox-vector-tile` Python package | 2.2.0, MIT | Build tool only, used by `tests/fixtures/generate_mvt_fixture.py` to produce the committed MVT test fixture (`tests/fixtures/tiny.mvt`) via a widely-used reference encoder | No — not vendored, not a runtime or build dependency; a dev-only tool invoked to generate one committed binary fixture. Not part of the shipped product. |
| Planetiler | 0.10.2 (`onthegomap/planetiler`, git `0e5588c4a6e8c29a270a33afe8df62027d889604`), Apache-2.0 | Host pack production only: Springfield OpenMapTiles and the Natural Earth `orcmaps-overview-1` world profile. Requires Java 21. | No — not vendored, not linked, not an OrcMaps runtime or ESP-IDF dependency. |
| go-pmtiles CLI | 1.28.2 (`protomaps/go-pmtiles` tag `v1.28.2`, commit `5898b17`), BSD-3-Clause | Host provisioning only: extract regional packs and derive z6/z7 world candidates from the z8 master | No — separately installed CLI under ignored local tools, not vendored, linked, or used at device runtime. Builders record the supplied version in manifests. |
| M5Unified | 0.2.20, MIT (via `m5stack/m5unified`) | **Tab5 example only** (`examples/m5stack-tab5`). Board bring-up matching OrcSDR. Not an OrcMaps core or M5GFX-adapter dependency. | No — fetched by ESP-IDF component manager for that example. |
| M5GFX | 0.2.27, MIT (via `m5stack/m5gfx`) | Optional display adapter plus Tab5, generic M5GFX, and LilyGO T-Display-S3 examples. Core `REQUIRES ""`. | No — fetched by ESP-IDF component manager for examples. |
| LilyGO T-Display-S3 board configuration | `Xinyuan-LilyGO/T-Display-S3` commit `ec889e789b3cf093412689a143f7f37b42b56af7` | MIT | Official panel and SD Shield pins plus the LovyanGFX ST7789 configuration used by `examples/lilygo-tdisplay-s3` | Yes — the small board configuration in the example is derived from LilyGO's `ST7789_Handler.h` and pin map; no LilyGO library is vendored or linked. |

## Resolved: MVT decoding

**Decision (2026-09-13): hand-rolled minimal protobuf/MVT reader, not a
library.** `src/tiles/mvt_decoder.cpp` implements just the wire-format
subset the MVT spec (`github.com/mapbox/vector-tile-spec`, an open
specification) actually needs — varint/LEN/32-bit/64-bit field parsing,
unknown-field skipping, and the four fixed MVT message shapes (Tile,
Layer, Feature, Value) — from the spec text directly, not from any
existing MVT/protobuf library's source. No protobuf runtime dependency
was added: a general-purpose protobuf parser was explicitly identified as
a cost worth avoiding for an embedded target that only ever needs to read
four fixed, well-known message shapes, never arbitrary `.proto` schemas.
The decoder is schema-agnostic (it doesn't know what a "road" layer or a
"class" attribute means) — see `docs/FORMAT_DECISION.md` "Deferred: tile
content schema" for the still-open decision this doesn't resolve.
