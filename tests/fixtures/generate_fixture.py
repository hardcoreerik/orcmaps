#!/usr/bin/env python3
"""Generate a tiny, spec-correct PMTiles v3 fixture for OrcMaps host tests.

Uses the official Protomaps `pmtiles` Python package (BSD-3-Clause,
https://github.com/protomaps/PMTiles) as an external build tool -- this
script is not shipped with OrcMaps and no PMTiles library code is copied
into this repository. It only produces the committed binary fixture at
tiny.pmtiles, which OrcMaps' own reader (src/tiles/pmtiles_reader.cpp) is
tested against independently.

The fixture is intentionally not real map data: tile payloads are short
marker strings identifying their z/x/y, so tests can assert on exact
contents without needing a vector-tile (MVT) decoder. Tile payloads are
stored uncompressed (`Compression.NONE`). The official `pmtiles` writer
may still gzip the directory even when `internal_compression` is
requested as NONE -- the committed `tiny.pmtiles` header reports gzip
internal compression, and `tests/host/test_pmtiles.cpp` asserts that.
Do not hand-edit the binary; regenerate and re-check the header fields
if the writer behavior changes.

Regenerate with: python tests/fixtures/generate_fixture.py
"""
from pathlib import Path

from pmtiles.tile import Compression, TileType, zxy_to_tileid
from pmtiles.writer import Writer

OUT = Path(__file__).parent / "tiny.pmtiles"


def tile_bytes(z: int, x: int, y: int) -> bytes:
    return f"orcmap-test-tile z={z} x={x} y={y}".encode("ascii")


def main() -> None:
    tiles: list[tuple[int, int, int, int]] = []  # (z, x, y, tileid)

    # z0: the single root tile.
    tiles.append((0, 0, 0, zxy_to_tileid(0, 0, 0)))

    # z1: all 4 tiles, so the reader can be tested against a full level.
    for x in range(2):
        for y in range(2):
            tiles.append((1, x, y, zxy_to_tileid(1, x, y)))

    # z2: a partial, non-contiguous set (skips some tiles) to exercise
    # RunLength=1 entries and gaps, not just dense runs.
    for x, y in [(0, 0), (1, 2), (3, 3)]:
        tiles.append((2, x, y, zxy_to_tileid(2, x, y)))

    tmp = OUT.with_suffix(".tmp")
    with open(tmp, "wb") as f:
        writer = Writer(f)
        for z, x, y, _tid in tiles:
            writer.write_tile(zxy_to_tileid(z, x, y), tile_bytes(z, x, y))
        writer.finalize(
            {
                "tile_type": TileType.MVT,
                "tile_compression": Compression.NONE,
                "internal_compression": Compression.NONE,
                "min_zoom": 0,
                "max_zoom": 2,
                "min_lon_e7": -1800000000,
                "min_lat_e7": -850511300,
                "max_lon_e7": 1800000000,
                "max_lat_e7": 850511300,
                "center_zoom": 0,
                "center_lon_e7": 0,
                "center_lat_e7": 0,
            },
            {
                "name": "orcmap-test-fixture",
                "description": "Synthetic fixture for OrcMaps host tests, not real map data",
                "attribution": "N/A - test fixture",
            },
        )
    tmp.replace(OUT)
    print(f"wrote {OUT} ({OUT.stat().st_size} bytes), {len(tiles)} tiles")


if __name__ == "__main__":
    main()
