#!/usr/bin/env python3
"""Acquire the pinned Natural Earth source bundle for OrcMaps world overview."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import struct
import sys
import time
from urllib.parse import urlparse
from urllib.request import urlopen
import zipfile

SCALES = {"110m", "50m", "10m"}
GEOMETRIES = {"point", "line", "polygon"}
REQUIRED_COMPONENTS = {".shp", ".shx", ".dbf", ".prj"}
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_definition(path: Path) -> dict:
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("schema_version") != 1 or data.get("provider") != "Natural Earth":
        raise ValueError("unsupported source definition")
    if not re.fullmatch(r"\d+\.\d+\.\d+", data.get("collection_version", "")):
        raise ValueError("invalid collection version")
    dt.date.fromisoformat(data.get("release_date", ""))
    if data.get("provenance_id") != "natural-earth":
        raise ValueError("world overview acquisition is restricted to natural-earth")
    datasets = data.get("datasets")
    if not isinstance(datasets, list) or not datasets:
        raise ValueError("datasets must be a non-empty list")
    seen: set[tuple[str, str]] = set()
    for item in datasets:
        required = {"scale", "category", "dataset", "theme_version", "geometry",
                    "use", "url", "archive", "expected_bytes", "sha256"}
        if not isinstance(item, dict) or not required.issubset(item):
            raise ValueError("dataset entry is missing required fields")
        key = (item["scale"], item["dataset"])
        parsed = urlparse(item["url"])
        expected_archive = f"ne_{item['scale']}_{item['dataset']}.zip"
        if key in seen or item["scale"] not in SCALES or item["geometry"] not in GEOMETRIES:
            raise ValueError(f"invalid or duplicate dataset: {key}")
        if parsed.scheme != "https" or parsed.hostname != "naturalearth.s3.amazonaws.com":
            raise ValueError(f"non-official source URL: {item['url']}")
        if item["archive"] != expected_archive or Path(parsed.path).name != expected_archive:
            raise ValueError(f"archive does not match dataset: {key}")
        if not isinstance(item["expected_bytes"], int) or item["expected_bytes"] <= 0:
            raise ValueError(f"invalid expected size: {key}")
        if item["sha256"] is not None and not SHA256_RE.fullmatch(item["sha256"]):
            raise ValueError(f"invalid SHA-256: {key}")
        seen.add(key)
    return data


def _safe_members(archive: zipfile.ZipFile, stem: str) -> list[zipfile.ZipInfo]:
    members = []
    for member in archive.infolist():
        path = PurePosixPath(member.filename)
        if path.is_absolute() or ".." in path.parts:
            raise ValueError(f"unsafe ZIP member: {member.filename}")
        if Path(path.name).stem == stem:
            members.append(member)
    suffixes = {Path(member.filename).suffix.lower() for member in members}
    missing = REQUIRED_COMPONENTS - suffixes
    if missing:
        raise ValueError(f"{stem} archive is missing: {', '.join(sorted(missing))}")
    return members


def _inspect_shapefile(directory: Path, stem: str) -> tuple[int, str]:
    dbf = next(directory.rglob(stem + ".dbf"))
    shp = next(directory.rglob(stem + ".shp"))
    with dbf.open("rb") as stream:
        stream.seek(4)
        feature_count = struct.unpack("<I", stream.read(4))[0]
    with shp.open("rb") as stream:
        stream.seek(32)
        shape_type = struct.unpack("<I", stream.read(4))[0]
    geometry = {1: "point", 3: "line", 5: "polygon", 8: "point",
                13: "line", 15: "polygon", 18: "point"}.get(shape_type)
    if geometry is None:
        raise ValueError(f"unsupported shapefile geometry type {shape_type}: {stem}")
    return feature_count, geometry


def acquire(definition: dict, destination: Path, *, force: bool = False,
            opener=urlopen) -> dict:
    started = time.monotonic()
    root = destination / definition["collection_version"]
    acquired = []
    checksum_lines = []
    for item in definition["datasets"]:
        scale_root = root / item["scale"]
        archive_path = scale_root / "archives" / item["archive"]
        dataset_root = scale_root / "datasets" / Path(item["archive"]).stem
        archive_path.parent.mkdir(parents=True, exist_ok=True)
        if archive_path.exists() and not force:
            actual_hash = sha256(archive_path)
            if (archive_path.stat().st_size != item["expected_bytes"] or
                    (item["sha256"] and actual_hash != item["sha256"])):
                raise FileExistsError(f"refusing to overwrite: {archive_path}")
        else:
            partial = archive_path.with_suffix(".zip.part")
            partial.unlink(missing_ok=True)
            try:
                with opener(item["url"]) as response, partial.open("wb") as output:
                    shutil.copyfileobj(response, output)
                if partial.stat().st_size != item["expected_bytes"]:
                    raise ValueError(f"size mismatch: {item['archive']}")
                actual_hash = sha256(partial)
                if item["sha256"] and actual_hash != item["sha256"]:
                    raise ValueError(f"SHA-256 mismatch: {item['archive']}")
                partial.replace(archive_path)
            finally:
                partial.unlink(missing_ok=True)
        actual_hash = sha256(archive_path)
        with zipfile.ZipFile(archive_path) as archive:
            stem = Path(item["archive"]).stem
            members = _safe_members(archive, stem)
            dataset_root.mkdir(parents=True, exist_ok=True)
            for member in members:
                archive.extract(member, dataset_root)
        feature_count, geometry = _inspect_shapefile(dataset_root, stem)
        if geometry != item["geometry"]:
            raise ValueError(f"geometry mismatch for {stem}: {geometry}")
        extracted_bytes = sum(path.stat().st_size for path in dataset_root.rglob("*") if path.is_file())
        relative_archive = archive_path.relative_to(root).as_posix()
        checksum_lines.append(f"{actual_hash}  {relative_archive}")
        acquired.append({**item, "sha256": actual_hash,
                         "feature_count": feature_count,
                         "extracted_bytes": extracted_bytes,
                         "extracted_path": dataset_root.relative_to(root).as_posix()})
    record = {
        "schema_version": 1,
        "provider": definition["provider"],
        "collection_version": definition["collection_version"],
        "release_date": definition["release_date"],
        "acquisition_date": dt.date.today().isoformat(),
        "official_release_url": definition["official_release_url"],
        "official_base_url": definition["official_base_url"],
        "provenance_id": definition["provenance_id"],
        "datasets": acquired,
        "total_downloaded_bytes": sum(item["expected_bytes"] for item in acquired),
        "total_extracted_bytes": sum(item["extracted_bytes"] for item in acquired),
        "elapsed_ms": round((time.monotonic() - started) * 1000),
    }
    root.mkdir(parents=True, exist_ok=True)
    (root / "SOURCE.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    (root / "SHA256SUMS.txt").write_text("\n".join(checksum_lines) + "\n", encoding="ascii")
    return record


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--definition", type=Path,
                        default=Path(__file__).with_name("world_overview_sources.json"))
    parser.add_argument("--destination", type=Path, required=True)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args(argv)
    definition = load_definition(args.definition)
    if args.dry_run:
        print(json.dumps({"destination": str(args.destination / definition["collection_version"]),
                          "total_bytes": sum(item["expected_bytes"] for item in definition["datasets"]),
                          "urls": [item["url"] for item in definition["datasets"]]}, indent=2))
        return 0
    record = acquire(definition, args.destination, force=args.force)
    print(json.dumps({"path": str(args.destination / definition["collection_version"]),
                      "downloaded_bytes": record["total_downloaded_bytes"],
                      "extracted_bytes": record["total_extracted_bytes"],
                      "elapsed_ms": record["elapsed_ms"]}))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, json.JSONDecodeError, zipfile.BadZipFile) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
