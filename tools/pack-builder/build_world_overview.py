#!/usr/bin/env python3
"""Build local Natural Earth z6/z7/z8 OrcMaps overview packs without networking."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


ROOT = Path(__file__).parents[2]
SOURCES = Path(__file__).with_name("world_overview_sources.json")

# The Web Mercator latitude limit, byte-identical to orcmap::kMercatorMaxLatDeg
# in include/orcmap/geo.hpp. ValidBounds() requires |lat| <= this value, so any
# rounding here makes the emitted manifest unloadable.
MERCATOR_MAX_LAT_DEG = 85.05112878
PROFILE = Path(__file__).with_name("world_overview_profile.json")
JAVA_PROFILE = Path(__file__).with_name("WorldOverviewProfile.java")
BOUNDS = (-1800000000, -850511288, 1800000000, 850511288)


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def load_profile(path: Path) -> dict:
    profile = load_json(path)
    if profile.get("schema_version") != "orcmaps-overview-1":
        raise ValueError("profile must declare orcmaps-overview-1")
    if set(profile.get("layers", {})) != {"land", "water", "waterway", "boundary", "place"}:
        raise ValueError("profile must declare the five OrcMaps overview layers")
    return profile


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_inputs(definition: dict, source_root: Path) -> dict[str, str]:
    if definition.get("collection_version") != "5.1.2":
        raise ValueError("Natural Earth collection must be 5.1.2")
    hashes = {}
    for item in definition.get("datasets", []):
        archive = source_root / item["scale"] / "archives" / item["archive"]
        if not archive.is_file() or sha256(archive).lower() != item["sha256"].lower():
            raise ValueError(f"archive SHA-256 mismatch: {archive}")
        stem = Path(item["archive"]).stem
        dataset = source_root / item["scale"] / "datasets" / stem
        for suffix in (".shp", ".shx", ".dbf", ".prj"):
            if not (dataset / f"{stem}{suffix}").is_file():
                raise ValueError(f"missing shapefile component: {dataset / f'{stem}{suffix}'}")
        hashes[item["archive"]] = item["sha256"].lower()
    return hashes


def validate_local_record(pinned: dict, local: dict) -> None:
    keys = ("scale", "archive", "sha256")
    expected = [{key: item[key] for key in keys} for item in pinned.get("datasets", [])]
    actual = [{key: item[key] for key in keys} for item in local.get("datasets", [])]
    if local.get("collection_version") != "5.1.2" or actual != expected:
        raise ValueError("local SOURCE.json does not match the pinned Natural Earth inputs")


def pack_id(max_zoom: int) -> str:
    return ("5.1.2__orcmaps-overview-1__overview__world__b" +
            "_".join(str(value) for value in BOUNDS) + f"__z0-{max_zoom}")


def command_output(command: list[str]) -> str:
    result = subprocess.run(command, check=True, text=True, capture_output=True)
    return (result.stdout + result.stderr).strip()


def validate_tools(java: str, javac: str, planetiler_jar: Path,
                   pmtiles_cli: Path) -> dict[str, str]:
    observed = {
        "java": command_output([java, "-version"]),
        "javac": command_output([javac, "-version"]),
        "planetiler": command_output([java, "-jar", str(planetiler_jar), "--version"]),
        "go_pmtiles": command_output([str(pmtiles_cli), "version"]),
    }
    if "21." not in observed["java"] or "21." not in observed["javac"]:
        raise ValueError("Java/Javac 21 required")
    if "Planetiler build version: 0.10.2" not in observed["planetiler"] or "0e5588c4a6e8c29a270a33afe8df62027d889604" not in observed["planetiler"]:
        raise ValueError("Planetiler 0.10.2 pin mismatch")
    if "pmtiles 1.28.2" not in observed["go_pmtiles"] or "5898b1719526e4dec27615387e663786ec9f3fcd" not in observed["go_pmtiles"]:
        raise ValueError("go-pmtiles v1.28.2 pin mismatch")
    return {
        "java": observed["java"].splitlines()[0],
        "javac": observed["javac"].splitlines()[0],
        "planetiler": "Planetiler 0.10.2, commit 0e5588c4a6e8c29a270a33afe8df62027d889604",
        "go_pmtiles": observed["go_pmtiles"].splitlines()[0],
    }


def write_sidecars(archive: Path, max_zoom: int, input_hashes: dict[str, str],
                   tool_versions: dict[str, str], commands: list[list[str]],
                   derived_from: str | None = None) -> None:
    output_hash = sha256(archive)
    manifest = {
        "manifest_version": 1,
        "pack_id": pack_id(max_zoom),
        "pack_version": "1",
        "display_name": f"OrcMaps World Overview z0-z{max_zoom}",
        "region_id": "world",
        "region_name": "World",
        # Must match the engine's kMercatorMaxLatDeg exactly. 85.0511288 is
        # LARGER than 85.05112878, so ValidBounds() -- and therefore runtime
        # discovery -- rejected every manifest this builder emitted. Rounding
        # a bound outward is never safe: llround(x * 1e7) still yields
        # +/-850511288, so pack_id identity is unchanged by the fix.
        "bounds": {"min_lon": -180.0, "min_lat": -MERCATOR_MAX_LAT_DEG,
                   "max_lon": 180.0, "max_lat": MERCATOR_MAX_LAT_DEG},
        "min_zoom": 0,
        "max_zoom": max_zoom,
        "content_profile": "overview",
        "pmtiles_version": 3,
        "schema_version": "orcmaps-overview-1",
        "source_snapshot": "5.1.2",
        "builder": "Planetiler / go-pmtiles",
        "builder_version": "Planetiler 0.10.2; go-pmtiles 1.28.2",
        "builder_commit": "0e5588c4a6e8c29a270a33afe8df62027d889604",
        "build_date": dt.date.today().isoformat(),
        "sources": [{"provenance_id": "natural-earth", "acquired": "2026-09-14",
                     "source_version": "5.1.2"}],
        "pack_class": "clean",
        "required_attribution": [],
        "attribution_links": ["https://www.naturalearthdata.com/"],
        "priority": 0,
        "size_bytes": archive.stat().st_size,
        "input_hashes": input_hashes,
        "output_sha256": output_hash,
        "build_tools": tool_versions,
        "build_commands": commands,
    }
    if derived_from:
        manifest["derived_from"] = derived_from
    archive.with_suffix(".manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    archive.with_suffix(".sha256").write_text(
        f"{output_hash}  {archive.name}\n", encoding="ascii")


def build_commands(profile: dict, sources: dict, source_root: Path, output_dir: Path,
                   planetiler_jar: Path, java: str, javac: str, pmtiles_cli: Path,
                   java_profile: Path = JAVA_PROFILE) -> list[list[str]]:
    del profile, sources
    classes = output_dir / "classes"
    master_partial = output_dir / "world-overview-z8.partial.pmtiles"
    classpath = os.pathsep.join((str(classes), str(planetiler_jar)))
    commands = [
        [javac, "-cp", str(planetiler_jar), "-d", str(classes), str(java_profile)],
        [java, "-cp", classpath, "WorldOverviewProfile",
         f"--profile_config={PROFILE}", f"--sources_config={SOURCES}",
         f"--source_root={source_root}", f"--output={master_partial}",
         "--maxzoom=8", "--tile_compression=gzip", "--force"],
    ]
    master = output_dir / "world-overview-z8.pmtiles"
    for zoom in (7, 6):
        commands.append([str(pmtiles_cli), "extract", str(master),
                         str(output_dir / f"world-overview-z{zoom}.partial.pmtiles"),
                         "--minzoom=0", f"--maxzoom={zoom}"])
    return commands


def parser() -> argparse.ArgumentParser:
    default_source = ROOT / "data/local/world-overview/natural-earth/5.1.2"
    default_output = ROOT / "data/local/world-overview/build"
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--source-root", type=Path, default=default_source)
    result.add_argument("--output-dir", type=Path, default=default_output)
    result.add_argument("--planetiler-jar", type=Path, default=ROOT / "data/local/planetiler.jar")
    result.add_argument("--java", default="java")
    result.add_argument("--javac", default="javac")
    result.add_argument("--pmtiles-cli", type=Path, required=True)
    result.add_argument("--dry-run", action="store_true")
    result.add_argument("--force", action="store_true")
    return result


def main(argv: list[str] | None = None) -> int:
    args = parser().parse_args(argv)
    profile = load_profile(PROFILE)
    sources = load_json(SOURCES)
    input_hashes = validate_inputs(sources, args.source_root)
    validate_local_record(sources, load_json(args.source_root / "SOURCE.json"))
    if not args.planetiler_jar.is_file():
        raise FileNotFoundError(args.planetiler_jar)
    if not args.pmtiles_cli.is_file():
        raise FileNotFoundError(args.pmtiles_cli)
    tool_versions = validate_tools(args.java, args.javac, args.planetiler_jar,
                                   args.pmtiles_cli)
    commands = build_commands(profile, sources, args.source_root, args.output_dir,
                              args.planetiler_jar, args.java, args.javac, args.pmtiles_cli)
    outputs = [args.output_dir / f"world-overview-z{zoom}.pmtiles" for zoom in (8, 7, 6)]
    if not args.force and any(path.exists() or path.with_suffix(".manifest.json").exists() for path in outputs):
        raise FileExistsError("refusing to replace an existing immutable overview pack")
    if args.dry_run:
        print(json.dumps({"commands": commands, "pack_ids": [pack_id(z) for z in (8, 7, 6)]}, indent=2))
        return 0
    args.output_dir.mkdir(parents=True, exist_ok=True)
    classes = args.output_dir / "classes"
    classes.mkdir(exist_ok=True)
    for partial in args.output_dir.glob("world-overview-z*.partial.pmtiles"):
        partial.unlink()
    try:
        subprocess.run(commands[0], check=True)
        subprocess.run(commands[1], check=True)
        os.replace(args.output_dir / "world-overview-z8.partial.pmtiles", outputs[0])
        for command, output in zip(commands[2:], outputs[1:]):
            subprocess.run(command, check=True)
            os.replace(Path(command[3]), output)
        for index, (zoom, output) in enumerate(zip((8, 7, 6), outputs)):
            write_sidecars(output, zoom, input_hashes, tool_versions, commands,
                           None if index == 0 else outputs[0].name)
    finally:
        shutil.rmtree(classes, ignore_errors=True)
        for partial in args.output_dir.glob("world-overview-z*.partial.pmtiles"):
            partial.unlink()
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
