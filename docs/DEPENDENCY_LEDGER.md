# Dependency ledger

Every third-party dependency incorporated into OrcMaps (code, not map data —
see [`DATA_AND_LICENSING.md`](DATA_AND_LICENSING.md) for data provenance) is
recorded here before it is added, per [`../LICENSING.md`](../LICENSING.md).

| Name | Version/commit | License | Used for | Code copied? |
|---|---|---|---|---|
| miniz (tinfl subset) | `richgel999/miniz` @ `master`, fetched 2026-09-13 | MIT | Inflate gzip/deflate-compressed PMTiles directories (and tile payloads, when a pack uses tile-level gzip) — bounded-memory, single-file, no external build dependency, works identically on host and ESP-IDF | Yes — vendored verbatim, decompression-only subset (`miniz_tinfl.h/.c`, `miniz_common.h`), unmodified, under `third_party/miniz/`. Full license text at `third_party/miniz/LICENSE`. |
| `pmtiles` Python package (protomaps) | 3.7.0, BSD-3-Clause | Build tool only, used by `tests/fixtures/generate_fixture.py` to produce the committed test fixture via the official reference implementation | No — not vendored, not a runtime or build dependency of OrcMaps itself; a dev-only tool invoked to generate one committed binary fixture. Not part of the shipped product. |

## Candidates under evaluation (not yet added)

| Name | License | Purpose if adopted | Notes |
|---|---|---|---|
| A vector-tile (MVT/protobuf) decoder | TBD | Decode vector tile payloads if vector PMTiles is selected | General-purpose protobuf parsing was flagged as a cost to avoid where possible — evaluate a minimal hand-rolled MVT reader vs. a library. Not needed for the container-format milestone (reading header/directory/tile-bytes) — only once actual map geometry decode starts. |
