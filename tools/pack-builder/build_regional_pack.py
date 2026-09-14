#!/usr/bin/env python3
"""Extract an immutable regional OrcMaps pack from an existing PMTiles archive."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys

MERCATOR_MAX_LAT = 85.0511287798066
IDENTITY_PART = re.compile(r"^[A-Za-z0-9.-]+$")


def parse_bbox(value: str) -> tuple[float, float, float, float]:
    try:
        bounds = tuple(float(part) for part in value.split(","))
    except ValueError as exc:
        raise argparse.ArgumentTypeError("bbox must be west,south,east,north") from exc
    if len(bounds) != 4 or not all(math.isfinite(v) for v in bounds):
        raise argparse.ArgumentTypeError("bbox must contain four finite numbers")
    west, south, east, north = bounds
    if not (-180 <= west <= 180 and -180 <= east <= 180 and
            -MERCATOR_MAX_LAT <= south <= north <= MERCATOR_MAX_LAT):
        raise argparse.ArgumentTypeError("bbox is outside Web Mercator bounds")
    return bounds  # type: ignore[return-value]


def geojson_bounds(path: Path) -> tuple[float, float, float, float]:
    data = json.loads(path.read_text(encoding="utf-8"))
    points: list[tuple[float, float]] = []

    def visit(value: object) -> None:
        if isinstance(value, dict):
            for key in ("geometry", "geometries", "features", "coordinates"):
                if key in value:
                    visit(value[key])
        elif isinstance(value, list):
            if len(value) >= 2 and all(isinstance(v, (int, float)) for v in value[:2]):
                points.append((float(value[0]), float(value[1])))
            else:
                for item in value:
                    visit(item)

    visit(data)
    if not points:
        raise ValueError("GeoJSON contains no coordinates")
    return parse_bbox(f"{min(x for x, _ in points)},{min(y for _, y in points)},"
                      f"{max(x for x, _ in points)},{max(y for _, y in points)}")


def e7(value: float) -> int:
    scaled = value * 10_000_000
    return math.floor(scaled + 0.5) if scaled >= 0 else math.ceil(scaled - 0.5)


def pack_id(args: argparse.Namespace, bounds: tuple[float, float, float, float]) -> str:
    parts = (args.source_snapshot, args.schema_version, args.content_profile,
             args.region_id)
    if not all(IDENTITY_PART.fullmatch(part) for part in parts):
        raise ValueError("identity fields allow only letters, digits, period, and hyphen")
    return ("__".join(part.lower() for part in parts) +
            "__b" + "_".join(str(e7(v)) for v in bounds) +
            f"__z{args.min_zoom}-{args.max_zoom}")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_provenance(args: argparse.Namespace) -> None:
    registry = Path(__file__).parents[2] / "data" / "sources"
    records = [json.loads(path.read_text(encoding="utf-8"))
               for path in registry.glob("*.json")]
    record = next((item for item in records if item.get("id") == args.provenance_id), None)
    if record is None:
        raise ValueError(f"unregistered provenance id: {args.provenance_id}")
    if record["policy"]["official_pack_allowed"] is not True:
        raise ValueError(f"source is not approved for official packs: {args.provenance_id}")
    if args.pack_class == "clean" and record["policy"]["clean_pack_allowed"] is not True:
        raise ValueError(f"source is not approved for clean packs: {args.provenance_id}")
    if record["rights"]["attribution_required"] and not args.attribution:
        raise ValueError(f"source requires local attribution text: {args.provenance_id}")


def write_sidecars(args: argparse.Namespace,
                   bounds: tuple[float, float, float, float]) -> None:
    output = args.output
    output_hash = sha256(output)
    manifest = {
        "manifest_version": 1,
        "pack_id": pack_id(args, bounds),
        "pack_version": args.pack_version,
        "display_name": args.display_name,
        "region_id": args.region_id,
        "region_name": args.region_name,
        "bounds": dict(zip(("min_lon", "min_lat", "max_lon", "max_lat"), bounds)),
        "min_zoom": args.min_zoom,
        "max_zoom": args.max_zoom,
        "content_profile": args.content_profile,
        "pmtiles_version": 3,
        "schema_version": args.schema_version,
        "source_snapshot": args.source_snapshot,
        "builder": args.builder,
        "builder_version": args.builder_version,
        "builder_commit": args.builder_commit,
        "build_date": args.build_date,
        "sources": [{"provenance_id": args.provenance_id,
                     "acquired": args.acquired,
                     "source_version": args.source_version}],
        "pack_class": args.pack_class,
        "required_attribution": args.attribution,
        "attribution_links": args.attribution_link,
        "priority": args.priority,
        "size_bytes": output.stat().st_size,
        "input_hashes": {args.source_label: args.source_sha256.lower()},
        "output_sha256": output_hash,
    }
    base = output.with_suffix("")
    base.with_suffix(".manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    base.with_suffix(".sha256").write_text(
        f"{output_hash}  {output.name}\n", encoding="ascii")


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--source", required=True, help="Local or remote source .pmtiles")
    area = p.add_mutually_exclusive_group(required=True)
    area.add_argument("--bbox", type=parse_bbox, help="west,south,east,north")
    area.add_argument("--geojson", type=Path, help="Polygon/Feature/FeatureCollection")
    p.add_argument("--min-zoom", type=int, required=True)
    p.add_argument("--max-zoom", type=int, required=True)
    p.add_argument("--content-profile", required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--region-id", required=True)
    p.add_argument("--region-name", required=True)
    p.add_argument("--display-name", required=True)
    p.add_argument("--source-snapshot", required=True)
    p.add_argument("--source-version", required=True)
    p.add_argument("--source-label", required=True)
    p.add_argument("--source-sha256", required=True)
    p.add_argument("--provenance-id", required=True)
    p.add_argument("--acquired", required=True)
    p.add_argument("--pack-version", required=True)
    p.add_argument("--pack-class", choices=("clean", "permissive", "open"), required=True)
    p.add_argument("--attribution", action="append", default=[])
    p.add_argument("--attribution-link", action="append", default=[])
    p.add_argument("--priority", type=int, default=0)
    p.add_argument("--schema-version", default="openmaptiles-3.16")
    p.add_argument("--pmtiles-cli", default="pmtiles")
    p.add_argument("--builder", default="orcmaps-pack-builder / go-pmtiles extract")
    p.add_argument("--builder-version", required=True)
    p.add_argument("--builder-commit", required=True)
    p.add_argument("--build-date", default=dt.date.today().isoformat())
    p.add_argument("--force", action="store_true")
    p.add_argument("--existing-output", action="store_true",
                   help="Only finalize PMTiles already produced by another builder")
    p.add_argument("--dry-run", action="store_true")
    return p


def main(argv: list[str] | None = None) -> int:
    args = parser().parse_args(argv)
    if not (0 <= args.min_zoom <= args.max_zoom <= 31):
        raise ValueError("zoom range must satisfy 0 <= min <= max <= 31")
    if not re.fullmatch(r"[0-9a-fA-F]{64}", args.source_sha256):
        raise ValueError("source SHA-256 must be 64 hexadecimal characters")
    if not re.fullmatch(r"[0-9a-fA-F]{40}", args.builder_commit):
        raise ValueError("builder commit must be 40 hexadecimal characters")
    if args.output.suffix.lower() != ".pmtiles":
        raise ValueError("output must end in .pmtiles")
    bounds = args.bbox if args.bbox else geojson_bounds(args.geojson)
    pack_id(args, bounds)
    validate_provenance(args)
    command = [args.pmtiles_cli, "extract", args.source, str(args.output),
               f"--minzoom={args.min_zoom}", f"--maxzoom={args.max_zoom}"]
    command.append(f"--bbox={','.join(format(v, '.15g') for v in bounds)}"
                   if args.bbox else f"--region={args.geojson}")
    if args.dry_run:
        print(json.dumps({"command": None if args.existing_output else command,
                          "pack_id": pack_id(args, bounds)}))
        return 0
    manifest_path = args.output.with_suffix(".manifest.json")
    checksum_path = args.output.with_suffix(".sha256")
    if (manifest_path.exists() or checksum_path.exists()) and not args.force:
        raise FileExistsError(f"refusing to replace immutable pack: {args.output}")
    if args.existing_output:
        if not args.output.is_file():
            raise FileNotFoundError(f"existing output not found: {args.output}")
        write_sidecars(args, bounds)
        print(args.output)
        return 0
    if args.output.exists() and not args.force:
        raise FileExistsError(f"refusing to replace immutable pack: {args.output}")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    partial = args.output.with_name(args.output.stem + ".partial.pmtiles")
    if partial.exists():
        partial.unlink()
    command[3] = str(partial)
    try:
        subprocess.run(command, check=True)
        os.replace(partial, args.output)
        write_sidecars(args, bounds)
    finally:
        partial.unlink(missing_ok=True)
    print(args.output)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
