#!/usr/bin/env python3
"""Build tests/fixtures/tiny-gzip.pmtiles: one gzip-compressed MVT tile
at z/x/y 0/0/0, using the committed tiny.mvt payload and the official
Protomaps writer. Synthetic -- not real map data.

Regenerate with: python tests/fixtures/generate_gzip_mvt_fixture.py
"""
import gzip
from pathlib import Path

from pmtiles.tile import Compression, TileType, zxy_to_tileid
from pmtiles.writer import Writer

ROOT = Path(__file__).parent
MVT = ROOT / "tiny.mvt"
GZ = ROOT / "tiny.mvt.gz"
OUT = ROOT / "tiny-gzip.pmtiles"


def main() -> None:
    payload = MVT.read_bytes()
    if not payload:
        raise SystemExit(f"missing or empty {MVT}")
    compressed = gzip.compress(payload)
    GZ.write_bytes(compressed)
    tmp = OUT.with_suffix(".tmp")
    with open(tmp, "wb") as f:
        writer = Writer(f)
        # The writer records tile_compression=GZIP but does not compress
        # payloads itself -- gzip here so GetTile returns RFC 1952 bytes.
        writer.write_tile(zxy_to_tileid(0, 0, 0), compressed)
        writer.finalize(
            {
                "tile_type": TileType.MVT,
                "tile_compression": Compression.GZIP,
                "internal_compression": Compression.NONE,
                "min_zoom": 0,
                "max_zoom": 0,
                "min_lon_e7": -1800000000,
                "min_lat_e7": -850511300,
                "max_lon_e7": 1800000000,
                "max_lat_e7": 850511300,
                "center_zoom": 0,
                "center_lon_e7": 0,
                "center_lat_e7": 0,
            },
            {
                "name": "orcmap-gzip-mvt-fixture",
                "description": "Synthetic gzip-compressed MVT in PMTiles, not real map data",
                "attribution": "N/A - test fixture",
            },
        )
    tmp.replace(OUT)
    print(
        f"wrote {OUT} ({OUT.stat().st_size} bytes) and {GZ.name} "
        f"({len(compressed)} bytes) from {MVT.name} ({len(payload)} bytes)"
    )


if __name__ == "__main__":
    main()
