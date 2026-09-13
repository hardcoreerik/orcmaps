#!/usr/bin/env python3
"""Generate a tiny, spec-correct MVT (Mapbox Vector Tile) fixture for
OrcMaps host tests.

Uses the `mapbox-vector-tile` Python package (MIT license,
https://github.com/tilezen/mapbox-vector-tile) as an external build tool --
this script is not shipped with OrcMaps and no library code is copied into
this repository. It only produces the committed binary fixture at
tiny.mvt, which OrcMaps' own decoder (src/tiles/mvt_decoder.cpp) is tested
against independently, exercising all four MVT value types (string, float/
double, integer, bool) and all three geometry types (point, linestring,
polygon) across three layers.

Regenerate with: python tests/fixtures/generate_mvt_fixture.py
"""
from pathlib import Path

import mapbox_vector_tile

OUT = Path(__file__).parent / "tiny.mvt"


def main() -> None:
    layers = [
        {
            "name": "water",
            "features": [
                {
                    "geometry": "POLYGON ((0 0, 4 0, 4 4, 0 4, 0 0))",
                    "properties": {"kind": "lake", "area": 12.5},
                },
            ],
        },
        {
            "name": "road",
            "features": [
                {
                    "geometry": "LINESTRING (0 0, 5 5, 10 0)",
                    "properties": {"class": "primary", "oneway": True, "lanes": 2},
                },
            ],
        },
        {
            "name": "place",
            "features": [
                {
                    "geometry": "POINT (5 5)",
                    "properties": {"name": "Testville", "rank": 3},
                },
            ],
        },
    ]

    data = mapbox_vector_tile.encode(layers, default_options={"quantize_bounds": None})

    tmp = OUT.with_suffix(".tmp")
    tmp.write_bytes(data)
    tmp.replace(OUT)
    print(f"wrote {OUT} ({len(data)} bytes), {len(layers)} layers")


if __name__ == "__main__":
    main()
