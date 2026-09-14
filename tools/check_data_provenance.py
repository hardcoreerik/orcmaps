#!/usr/bin/env python3
"""Deterministic, standard-library checks for the OrcMaps data provenance registry.

Sibling to tools/check_documentation_truth.py, which stays responsible for
documentation consistency. This script is responsible for a different
question: is every dataset we might use actually approved, under terms we
actually understand, with real evidence? See docs/DATA_AND_LICENSING.md and
docs/DATA_PROVENANCE_REGISTRY.md for the policy this script enforces.

This is not a legal-analysis engine. It enforces decisions that have
already been made and recorded -- it does not determine what a license
means, only whether a registry record is internally consistent with the
license class it claims and with our own policy rules.
"""

from __future__ import annotations

import argparse
import dataclasses
import json
import os
import re
import sys
from pathlib import Path

SOURCES_DIR = "data/sources"
# Committed, CI-visible manifests live here. Generated packs under
# data/local/ are intentionally ignored.
PACK_MANIFEST_GLOBS = (
    "data/packs/*.manifest.json",
    "examples/*/test-pack/*.manifest.json",
)

LICENSE_CLASSES = {
    "CC0", "PDDL", "PUBLIC_DOMAIN_VERIFIED", "US_FEDERAL_PUBLIC_DOMAIN",
    "PERMISSIVE_ATTRIBUTION", "CDLA_PERMISSIVE", "ODBL", "PROPRIETARY_APPROVED",
    "NONCOMMERCIAL", "NO_DERIVATIVES", "UNKNOWN",
}
CONFIDENCE_VALUES = {"CONFIRMED", "REVIEW_REQUIRED"}

RIGHTS_BOOL_FIELDS = (
    "commercial_use_allowed", "redistribution_allowed", "modification_allowed",
    "attribution_required", "share_alike_required", "source_disclosure_required",
)

_PD_CLASSES = {"CC0", "PDDL", "PUBLIC_DOMAIN_VERIFIED", "US_FEDERAL_PUBLIC_DOMAIN"}

# The single source of truth for what each license_class implies. A value of
# True/False here means the checker FORCES that field to that value; None
# means "per-record" (the field must still be a bool, but any value is
# accepted). See docs/DATA_PROVENANCE_REGISTRY.md "What each class implies"
# for the human-readable form of this same table -- keep them in sync.
LICENSE_CLASS_CONSTRAINTS: dict[str, dict[str, bool | None]] = {
    **{cls: {
        "commercial_use_allowed": True, "redistribution_allowed": True,
        "modification_allowed": True, "attribution_required": None,
        "share_alike_required": False,
    } for cls in _PD_CLASSES},
    "PERMISSIVE_ATTRIBUTION": {"share_alike_required": False},
    "CDLA_PERMISSIVE": {"share_alike_required": False},
    "ODBL": {
        "commercial_use_allowed": True, "redistribution_allowed": True,
        "modification_allowed": True, "attribution_required": True,
        "share_alike_required": True,
    },
    "PROPRIETARY_APPROVED": {"share_alike_required": False},
    "NONCOMMERCIAL": {"commercial_use_allowed": False},
    "NO_DERIVATIVES": {"modification_allowed": False},
    "UNKNOWN": {},
}

# clean_pack_allowed=true is only ever permitted for these classes -- every
# other class must have clean_pack_allowed=false, no per-record exception.
_CLEAN_ELIGIBLE_CLASSES = _PD_CLASSES

_URL_RE = re.compile(r"^https?://")
_DATE_RE = re.compile(r"^\d{4}-\d{2}-\d{2}$")


@dataclasses.dataclass(frozen=True)
class Diagnostic:
    level: str
    code: str
    path: str
    message: str


@dataclasses.dataclass
class Report:
    errors: list[Diagnostic] = dataclasses.field(default_factory=list)
    warnings: list[Diagnostic] = dataclasses.field(default_factory=list)
    passes: list[str] = dataclasses.field(default_factory=list)

    def add(self, level: str, code: str, path: str, message: str) -> None:
        item = Diagnostic(level, code, path, message)
        (self.errors if level == "ERROR" else self.warnings).append(item)


def _get(record: dict, *path: str):
    node = record
    for key in path:
        if not isinstance(node, dict) or key not in node:
            return None
        node = node[key]
    return node


def _require(record: dict, report: Report, rel: str, *path: str) -> bool:
    if _get(record, *path) is None:
        report.add("ERROR", "missing-field", rel, f"missing required field: {'.'.join(path)}")
        return False
    return True


def _load_records(root: Path, report: Report) -> dict[str, tuple[dict, str]]:
    """Returns {id: (record, relative_path)}. Duplicate ids are reported and
    only the first occurrence is kept for further checks."""
    sources_dir = root / SOURCES_DIR
    records: dict[str, tuple[dict, str]] = {}
    if not sources_dir.is_dir():
        report.add("ERROR", "no-registry", SOURCES_DIR, "data provenance registry directory is missing")
        return records
    for path in sorted(sources_dir.glob("*.json")):
        rel = path.relative_to(root).as_posix()
        try:
            record = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc:
            report.add("ERROR", "invalid-json", rel, f"not valid JSON: {exc}")
            continue
        if not isinstance(record, dict):
            report.add("ERROR", "invalid-json", rel, "top-level JSON value must be an object")
            continue
        source_id = record.get("id")
        if not isinstance(source_id, str) or not source_id:
            report.add("ERROR", "missing-field", rel, "missing required field: id")
            continue
        if source_id != path.stem:
            report.add(
                "ERROR", "id-filename-mismatch", rel,
                f"record id '{source_id}' does not match filename '{path.stem}.json'",
            )
        if source_id in records:
            report.add(
                "ERROR", "duplicate-id", rel,
                f"duplicate source id '{source_id}' (also in {records[source_id][1]})",
            )
            continue
        records[source_id] = (record, rel)
    return records


def _check_required_fields(records: dict[str, tuple[dict, str]], report: Report) -> None:
    required_paths = [
        ("name",), ("publisher",),
        ("source", "url"), ("source", "retrieved"),
        ("rights", "license"), ("rights", "license_class"),
        *[("rights", f) for f in RIGHTS_BOOL_FIELDS],
        ("evidence", "rights_url"), ("evidence", "reviewed_date"), ("evidence", "confidence"),
        ("policy", "official_pack_allowed"), ("policy", "clean_pack_allowed"),
    ]
    ok = True
    for source_id, (record, rel) in records.items():
        for path in required_paths:
            if not _require(record, report, rel, *path):
                ok = False
        for field in RIGHTS_BOOL_FIELDS:
            value = _get(record, "rights", field)
            if value is not None and not isinstance(value, bool):
                report.add("ERROR", "wrong-type", rel, f"rights.{field} must be a boolean, got {value!r}")
                ok = False
        for path in (("policy", "official_pack_allowed"), ("policy", "clean_pack_allowed")):
            value = _get(record, *path)
            if value is not None and not isinstance(value, bool):
                report.add("ERROR", "wrong-type", rel, f"{'.'.join(path)} must be a boolean, got {value!r}")
                ok = False
    if records and ok:
        report.passes.append(f"All {len(records)} registry record(s) have every required field")


def _check_license_class(records: dict[str, tuple[dict, str]], report: Report) -> None:
    ok = True
    for source_id, (record, rel) in records.items():
        cls = _get(record, "rights", "license_class")
        if cls is None:
            continue
        if cls not in LICENSE_CLASSES:
            report.add("ERROR", "unknown-license-class", rel, f"license_class '{cls}' is not a recognized class")
            ok = False
            continue
        confidence = _get(record, "evidence", "confidence")
        if confidence is not None and confidence not in CONFIDENCE_VALUES:
            report.add(
                "ERROR", "unknown-confidence", rel,
                f"evidence.confidence '{confidence}' must be CONFIRMED or REVIEW_REQUIRED",
            )
            ok = False

        constraints = LICENSE_CLASS_CONSTRAINTS.get(cls, {})
        for field, forced in constraints.items():
            if forced is None:
                continue
            actual = _get(record, "rights", field)
            if actual is not None and actual != forced:
                report.add(
                    "ERROR", "license-class-mismatch", rel,
                    f"license_class '{cls}' requires rights.{field} == {forced}, got {actual}",
                )
                ok = False

        clean_allowed = _get(record, "policy", "clean_pack_allowed")
        if clean_allowed is True and cls not in _CLEAN_ELIGIBLE_CLASSES:
            report.add(
                "ERROR", "clean-pack-not-eligible", rel,
                f"license_class '{cls}' can never have policy.clean_pack_allowed=true "
                f"(only {sorted(_CLEAN_ELIGIBLE_CLASSES)} are Clean-pack eligible)",
            )
            ok = False

        official_allowed = _get(record, "policy", "official_pack_allowed")
        if cls == "UNKNOWN" and official_allowed is True:
            report.add(
                "ERROR", "unknown-not-official", rel,
                "license_class UNKNOWN can never have policy.official_pack_allowed=true",
            )
            ok = False
        if cls == "NONCOMMERCIAL" and official_allowed is True:
            report.add(
                "ERROR", "noncommercial-not-official", rel,
                "license_class NONCOMMERCIAL can never have policy.official_pack_allowed=true "
                "(standard official packs assume commercial use is allowed)",
            )
            ok = False

        if confidence == "REVIEW_REQUIRED":
            if official_allowed is True:
                report.add(
                    "ERROR", "review-required-not-official", rel,
                    "evidence.confidence is REVIEW_REQUIRED, so policy.official_pack_allowed must be false",
                )
                ok = False
            if clean_allowed is True:
                report.add(
                    "ERROR", "review-required-not-clean", rel,
                    "evidence.confidence is REVIEW_REQUIRED, so policy.clean_pack_allowed must be false",
                )
                ok = False
    if records and ok:
        report.passes.append("All registry records' policy flags are consistent with their license_class")


def _check_evidence(records: dict[str, tuple[dict, str]], report: Report) -> None:
    ok = True
    for source_id, (record, rel) in records.items():
        for path in (("source", "url"), ("evidence", "rights_url")):
            value = _get(record, *path)
            if isinstance(value, str) and not _URL_RE.match(value):
                report.add("ERROR", "bad-url", rel, f"{'.'.join(path)} does not look like a URL: {value!r}")
                ok = False
        for path in (("source", "retrieved"), ("evidence", "reviewed_date")):
            value = _get(record, *path)
            if isinstance(value, str) and not _DATE_RE.match(value):
                report.add("ERROR", "bad-date", rel, f"{'.'.join(path)} must be YYYY-MM-DD, got {value!r}")
                ok = False
    if records and ok:
        report.passes.append("All registry records have well-formed evidence URLs and dates")


def _check_pack_manifests(
    root: Path, records: dict[str, tuple[dict, str]], report: Report
) -> None:
    checked = 0
    ok = True
    paths = sorted({path for pattern in PACK_MANIFEST_GLOBS for path in root.glob(pattern)})
    for path in paths:
        checked += 1
        rel = path.relative_to(root).as_posix()
        try:
            manifest = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc:
            report.add("ERROR", "invalid-manifest-json", rel, f"not valid JSON: {exc}")
            ok = False
            continue
        if not isinstance(manifest, dict):
            report.add("ERROR", "invalid-manifest-json", rel, "top-level JSON value must be an object")
            ok = False
            continue

        sources = manifest.get("sources")
        if not isinstance(sources, list):
            report.add("ERROR", "missing-field", rel, "missing required field: sources")
            ok = False
            continue
        if not sources:
            report.add(
                "ERROR", "missing-provenance-id", rel,
                "manifest must reference at least one provenance source",
            )
            ok = False
            continue

        for source in sources:
            provenance_id = source.get("provenance_id") if isinstance(source, dict) else None
            if not isinstance(provenance_id, str) or not provenance_id:
                report.add(
                    "ERROR", "missing-provenance-id", rel,
                    "every manifest source must have a non-empty provenance_id",
                )
                ok = False
                continue
            registered = records.get(provenance_id)
            if registered is None:
                report.add(
                    "ERROR", "unknown-provenance-id", rel,
                    f"source provenance_id '{provenance_id}' is not registered",
                )
                ok = False
                continue

            record = registered[0]
            if _get(record, "policy", "official_pack_allowed") is not True:
                report.add(
                    "ERROR", "source-not-official", rel,
                    f"source '{provenance_id}' is not allowed in official packs",
                )
                ok = False
            if (manifest.get("pack_class") == "clean" and
                    _get(record, "policy", "clean_pack_allowed") is not True):
                report.add(
                    "ERROR", "source-not-clean", rel,
                    f"source '{provenance_id}' is not allowed in clean packs",
                )
                ok = False

    if checked and ok:
        report.passes.append(f"All {checked} committed pack manifest(s) reference approved sources")


def run_checks(root: Path) -> Report:
    root = root.resolve()
    report = Report()
    records = _load_records(root, report)
    if records:
        report.passes.append(f"Loaded {len(records)} unique data provenance record(s)")
    _check_required_fields(records, report)
    _check_license_class(records, report)
    _check_evidence(records, report)
    _check_pack_manifests(root, records, report)
    return report


def _render(report: Report) -> str:
    lines = ["OrcMaps Data Provenance Check", "=============================="]
    lines.extend(f"PASS  {message}" for message in report.passes)
    for item in report.warnings:
        lines.append(f"WARN  {item.path}: {item.message}")
    for item in report.errors:
        lines.append(f"ERROR {item.path}: {item.message}")
    lines.extend(("", f"{len(report.errors)} errors", f"{len(report.warnings)} warnings"))
    return "\n".join(lines)


def _github_output(report: Report, rendered: str) -> None:
    for item in report.errors + report.warnings:
        level = "error" if item.level == "ERROR" else "warning"
        print(f"::{level} file={item.path}::{item.message}")
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as handle:
            handle.write("```text\n" + rendered + "\n```\n")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    report = run_checks(args.root)
    rendered = _render(report)
    if args.json:
        print(json.dumps(dataclasses.asdict(report), indent=2))
    else:
        print(rendered)
    if os.environ.get("GITHUB_ACTIONS") == "true":
        _github_output(report, rendered)
    return 1 if report.errors else 0


if __name__ == "__main__":
    sys.exit(main())
