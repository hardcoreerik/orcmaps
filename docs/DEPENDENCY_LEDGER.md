# Dependency ledger

Every third-party dependency incorporated into OrcMaps (code, not map data —
see [`DATA_AND_LICENSING.md`](DATA_AND_LICENSING.md) for data provenance) is
recorded here before it is added, per [`../LICENSING.md`](../LICENSING.md).

| Name | Version/commit | License | Used for | Code copied? |
|---|---|---|---|---|
| miniz (tinfl subset) | `richgel999/miniz` @ `master`, fetched 2026-09-13 | MIT | Inflate gzip/deflate-compressed PMTiles directories (and tile payloads, when a pack uses tile-level gzip) — bounded-memory, single-file, no external build dependency, works identically on host and ESP-IDF | Yes — vendored verbatim, decompression-only subset (`miniz_tinfl.h/.c`, `miniz_common.h`), unmodified, under `third_party/miniz/`. Full license text at `third_party/miniz/LICENSE`. |
| `pmtiles` Python package (protomaps) | 3.7.0, BSD-3-Clause | Build tool only, used by `tests/fixtures/generate_fixture.py` to produce the committed test fixture via the official reference implementation | No — not vendored, not a runtime or build dependency of OrcMaps itself; a dev-only tool invoked to generate one committed binary fixture. Not part of the shipped product. |
| `mapbox-vector-tile` Python package | 2.2.0, MIT | Build tool only, used by `tests/fixtures/generate_mvt_fixture.py` to produce the committed MVT test fixture (`tests/fixtures/tiny.mvt`) via a widely-used reference encoder | No — not vendored, not a runtime or build dependency; a dev-only tool invoked to generate one committed binary fixture. Not part of the shipped product. |

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
