#!/usr/bin/env python3
"""Reject graphics- or board-specific dependencies in the world-pack/core path."""

from pathlib import Path
import sys


ROOT = Path(__file__).parents[1]
FILES = (
    "tools/pack-builder/WorldOverviewProfile.java",
    "tools/pack-builder/build_world_overview.py",
    "tools/pack-builder/world_overview_profile.json",
    "include/orcmap/pack.hpp",
    "src/core/pack.cpp",
    "include/orcmap/experimental/mvt_classify.hpp",
    "src/tiles/mvt_classify_experimental.cpp",
)
FORBIDDEN = ("M5GFX", "M5Unified", "LVGL", "Tab5", "OrcSDR")


def main() -> int:
    failures = []
    for relative in FILES:
        text = (ROOT / relative).read_text(encoding="utf-8")
        failures.extend(f"{relative}: contains {token}" for token in FORBIDDEN if token in text)
    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    print("world overview builder/profile/core path is graphics-framework independent")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
