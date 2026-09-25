#!/usr/bin/env python3
"""Fail if OrcMaps' miniz calls can reach an ESP-IDF ROM copy of miniz.

ESP-IDF ROM linker scripts define tinfl_*, tdefl_* and some mz_* symbols at
absolute addresses, and those beat the bundled objects at link time. OrcMaps
v0.2.0 sized its inflater state from the bundled miniz but called the ROM
inflater (a different layout) and corrupted the heap on an ESP32-P4. The
bundled miniz is now renamed with an orcmap_ prefix
(third_party/miniz/orcmap_miniz_prefix.h); this check proves it on a linked
image, which host builds cannot, because hosts have no ROM.

Usage, on an ESP-IDF build directory (tests/esp_idf_link is made for this):

    python tools/check_miniz_symbols.py BUILD_DIR [--nm NM]

Checks, all of which must hold:
  1. The OrcMaps component archive defines no unprefixed miniz symbol.
  2. No OrcMaps object references an unprefixed miniz symbol.
  3. The archive references at least one orcmap_ miniz symbol (so the gate
     cannot pass vacuously on an app that links no inflater).
  4. In the linked ELF, every orcmap_ miniz symbol OrcMaps references is
     defined in code (nm type T/t), never absolute (A).
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

MINIZ = re.compile(r"^(tinfl_|tdefl_|mz_|miniz_def_)")
PREFIX = "orcmap_"


def nm_lines(nm: str, path: Path, *flags: str) -> list[tuple[str, str]]:
    """(type, name) pairs from nm; archive member headers are skipped."""
    out = subprocess.run([nm, *flags, str(path)], check=True, text=True,
                         capture_output=True).stdout
    pairs = []
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 2 and len(parts[-2]) == 1:
            pairs.append((parts[-2], parts[-1]))
    return pairs


def find_nm(build: Path, override: str | None) -> str:
    if override:
        return override
    description = build / "project_description.json"
    if description.is_file():
        prefix = json.loads(description.read_text()).get("monitor_toolprefix", "")
        if prefix and shutil.which(prefix + "nm"):
            return prefix + "nm"
    raise SystemExit("error: cannot find the target nm; pass --nm "
                     "(e.g. riscv32-esp-elf-nm) or export ESP-IDF first")


def find_component_archive(build: Path) -> Path:
    # The component is named after the checkout folder (OrcMaps, orcmaps, ...)
    # or after the dependency key when fetched by the component manager.
    candidates = [p for p in build.glob("esp-idf/*/lib*.a")
                  if (p.parent.name.lower().startswith("orcmap"))]
    if len(candidates) != 1:
        raise SystemExit(f"error: expected one OrcMaps archive under {build}/esp-idf, "
                         f"found {[str(p) for p in candidates]}")
    return candidates[0]


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("build_dir", type=Path)
    parser.add_argument("--nm", help="target nm (default: from project_description.json)")
    args = parser.parse_args(argv)

    build = args.build_dir
    nm = find_nm(build, args.nm)
    archive = find_component_archive(build)
    elfs = sorted(build.glob("*.elf"))
    if len(elfs) != 1:
        raise SystemExit(f"error: expected one ELF in {build}, found {elfs}")
    elf = elfs[0]

    failures: list[str] = []
    defined = {name for kind, name in nm_lines(nm, archive, "-g", "--defined-only")
               if kind != "U"}
    undefined = {name for kind, name in nm_lines(nm, archive, "-u") if kind == "U"}

    for name in sorted(defined):
        if MINIZ.match(name):
            failures.append(f"{archive.name} defines unprefixed miniz symbol {name}")
    for name in sorted(undefined):
        if MINIZ.match(name):
            failures.append(f"{archive.name} references unprefixed miniz symbol {name} "
                            "(would resolve to ESP ROM miniz if the ROM exports it)")

    used = sorted(name for name in undefined
                  if name.startswith(PREFIX) and MINIZ.match(name[len(PREFIX):]))
    if not used:
        failures.append(f"{archive.name} references no orcmap_ miniz symbol; "
                        "the app links no inflater, so this check proves nothing")

    elf_kinds = {name: kind for kind, name in nm_lines(nm, elf)}
    for name in used:
        kind = elf_kinds.get(name)
        if kind not in ("T", "t"):
            failures.append(f"{elf.name}: {name} is {kind or 'missing'}, expected T")

    rom = sorted(name for name, kind in elf_kinds.items()
                 if kind == "A" and MINIZ.match(name))
    print(f"archive: {archive}")
    print(f"elf:     {elf}")
    print(f"ROM-absolute miniz symbols in image (unused by OrcMaps): {len(rom)}")
    for name in used:
        print(f"  {elf_kinds.get(name, '?')} {name}")
    if failures:
        print("FAIL", file=sys.stderr)
        for failure in failures:
            print(f"  {failure}", file=sys.stderr)
        return 1
    print(f"PASS: OrcMaps uses {len(used)} bundled miniz symbols, none ROM-absolute")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
