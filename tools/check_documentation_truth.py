#!/usr/bin/env python3
"""Deterministic, standard-library checks for high-confidence documentation drift.

Adapted from hardcoreerik/OrcSDR's tools/check_documentation_truth.py for
OrcMaps' own structure and claims. See PROJECT_TRUTH.md "Document
authority" for why this exists: durable decisions must live in the
canonical docs, and this script's whole job is to catch when those docs
stop matching the repository they describe.
"""

from __future__ import annotations

import argparse
import dataclasses
import json
import os
import re
import sys
from pathlib import Path
from urllib.parse import unquote


CURRENT_DOCS = (
    "README.md",
    "PROJECT_TRUTH.md",
    "ARCHITECTURE.md",
    "ROADMAP.md",
    "STATUS.md",
    "LICENSING.md",
    "CONTRIBUTING.md",
)
# Historical/superseded docs are exempt from the prompt-residue and
# resolved-claim checks below, same as OrcSDR's HISTORICAL_DOCS. Empty for
# now -- OrcMaps has no historical docs yet, but the mechanism stays ready
# rather than being bolted on later once one exists.
HISTORICAL_DOCS: tuple[str, ...] = ()

PROMPT_PATTERNS = (
    re.compile(r"\byour job is to\b", re.I),
    re.compile(r"\byou are (?:codex|claude)\b", re.I),
    re.compile(r"\bimplement the following\b", re.I),
    re.compile(r"<instructions?>", re.I),
)

# Known-obsolete current-state claims that must never resurface once fixed.
RESOLVED_CLAIM_GUARDS: dict[str, tuple[str, ...]] = {
    # README used to say the M5GFX retarget was "not yet built".
    "README.md": (r"M5GFX retarget",),
    # examples/m5gfx must consume the adapter via OrcMaps' exported include
    # path, not an in-repo relative INCLUDE_DIRS shortcut.
    "examples/m5gfx/main/CMakeLists.txt": (r"\.\./\.\./\.\./adapters",),
}


@dataclasses.dataclass(frozen=True)
class Diagnostic:
    level: str
    code: str
    path: str
    line: int
    message: str


@dataclasses.dataclass
class Report:
    errors: list[Diagnostic] = dataclasses.field(default_factory=list)
    warnings: list[Diagnostic] = dataclasses.field(default_factory=list)
    passes: list[str] = dataclasses.field(default_factory=list)

    def add(self, level: str, code: str, path: str, line: int, message: str) -> None:
        item = Diagnostic(level, code, path, line, message)
        (self.errors if level == "ERROR" else self.warnings).append(item)


def _text(root: Path, relative: str) -> str | None:
    path = root / relative
    return path.read_text(encoding="utf-8") if path.is_file() else None


def _line(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


_CODE_FENCE_PATTERN = re.compile(r"```.*?```", re.S)


def _mask_code_fences(text: str) -> str:
    """Replaces the contents of fenced code blocks with spaces (preserving
    length/line numbers) so checks that scan prose for claims -- like the
    component-version check -- don't misread example/sample values inside
    a ```json ...``` block as a real claim about this repository.
    """
    def _blank(match: re.Match) -> str:
        return re.sub(r"[^\n]", " ", match.group(0))
    return _CODE_FENCE_PATTERN.sub(_blank, text)


def _current_markdown(root: Path) -> list[Path]:
    paths = [root / item for item in CURRENT_DOCS]
    docs_dir = root / "docs"
    if docs_dir.is_dir():
        paths.extend(docs_dir.glob("*.md"))
    return sorted({path for path in paths if path.is_file()})


def _check_ci_claims(root: Path, report: Report) -> None:
    workflows = list((root / ".github/workflows").glob("*.y*ml"))
    if not workflows:
        return
    pattern = re.compile(r"\b(?:orcmaps\s+has\s+)?no\s+(?:github actions\s+)?ci\b", re.I)
    for path in _current_markdown(root):
        text = path.read_text(encoding="utf-8")
        for match in pattern.finditer(text):
            report.add(
                "ERROR", "no-ci", path.relative_to(root).as_posix(),
                _line(text, match.start()),
                "unqualified no-CI claim conflicts with existing workflows",
            )
    if not any(item.code == "no-ci" for item in report.errors):
        report.passes.append(f"CI description matches {len(workflows)} workflow files")


def _check_links(root: Path, report: Report) -> None:
    link_re = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
    for path in _current_markdown(root):
        text = path.read_text(encoding="utf-8")
        for match in link_re.finditer(text):
            raw = match.group(1).strip().split()[0].strip("<>")
            if not raw or raw.startswith(("#", "http://", "https://", "mailto:")):
                continue
            target = unquote(raw.split("#", 1)[0].split("?", 1)[0])
            resolved = (root / target.lstrip("/")) if target.startswith("/") else (path.parent / target)
            if not resolved.exists():
                report.add(
                    "ERROR", "local-link", path.relative_to(root).as_posix(),
                    _line(text, match.start()),
                    f"local link target does not exist: {raw}",
                )
    if not any(item.code == "local-link" for item in report.errors):
        report.passes.append("Repository-local Markdown links resolve")


# Backtick-quoted repository paths with a file extension, e.g. `src/core/geo.cpp`.
# Deliberately excludes anything containing `{`/`}` or `*` -- those are
# shorthand for multiple files (e.g. `file_byte_source.{hpp,cpp}`) or globs,
# not literal paths, and must not be existence-checked as one.
_FILE_REF_PATTERN = re.compile(
    r"`((?:include|src|adapters|tools|tests|docs|examples|third_party|\.github)"
    r"/[^`*{}<>]+\.[A-Za-z0-9]+)`"
)


def _check_file_references(root: Path, report: Report) -> None:
    for path in _current_markdown(root):
        text = path.read_text(encoding="utf-8")
        for match in _FILE_REF_PATTERN.finditer(text):
            reference = match.group(1)
            if not (root / reference).exists():
                report.add(
                    "ERROR", "file-reference", path.relative_to(root).as_posix(),
                    _line(text, match.start()),
                    f"referenced repository file does not exist: {reference}",
                )
    if not any(item.code == "file-reference" for item in report.errors):
        report.passes.append("High-confidence documented file references resolve")


_TABLE_ROW_PATTERN = re.compile(r"^\|([^|\n]+)\|([^|\n]+)\|([^|\n]+)\|\s*$", re.M)
_BACKTICK_PATH_PATTERN = re.compile(r"`([^`]+)`")


def _check_component_table_dirs(root: Path, report: Report) -> None:
    """Cross-checks ARCHITECTURE.md's "Major components" table against the
    actual filesystem: a row claiming a directory is "(empty dir)" must
    point at a directory with no files in it, and a row claiming a
    directory-shaped path is "Implemented" must point at a directory that
    actually has files. This is the OrcMaps analogue of OrcSDR's
    screen/dashboard-enum drift check -- comparing a documented claim
    against ground truth, not just checking the doc is internally
    well-formed.
    """
    architecture = _text(root, "ARCHITECTURE.md")
    if architecture is None:
        return
    checked_any = False
    for match in _TABLE_ROW_PATTERN.finditer(architecture):
        _label, paths_cell, status_cell = match.groups()
        status = status_cell.strip()
        paths = _BACKTICK_PATH_PATTERN.findall(paths_cell)
        dir_paths = [p for p in paths if p.endswith("/") and "*" not in p and "{" not in p]
        if not dir_paths:
            continue
        claims_empty = bool(re.search(r"\(empty dirs?\)", status, re.I))
        claims_implemented = status.lower().startswith("implemented") and not claims_empty
        if not (claims_empty or claims_implemented):
            continue
        checked_any = True
        for rel in dir_paths:
            directory = root / rel
            has_files = directory.is_dir() and any(p.is_file() for p in directory.rglob("*"))
            if claims_empty and has_files:
                report.add(
                    "ERROR", "component-table-drift", "ARCHITECTURE.md",
                    _line(architecture, match.start()),
                    f"'{rel}' is documented as an empty/not-implemented directory "
                    f"but contains files -- update ARCHITECTURE.md's status for this row",
                )
            elif claims_implemented and not has_files:
                report.add(
                    "ERROR", "component-table-drift", "ARCHITECTURE.md",
                    _line(architecture, match.start()),
                    f"'{rel}' is documented as Implemented but the directory has no "
                    f"files -- update ARCHITECTURE.md's status for this row",
                )
    if checked_any and not any(item.code == "component-table-drift" for item in report.errors):
        report.passes.append("ARCHITECTURE.md component table matches directory contents")


_TEST_FUNCTION_PATTERN = re.compile(r"^void (Test\w+)\(", re.M)
_TEST_COUNT_CLAIM_PATTERN = re.compile(r"(\d+)\s+test functions?", re.I)


def _check_test_function_count(root: Path, report: Report) -> None:
    """STATUS.md/ARCHITECTURE.md cite an exact count of host test functions
    (e.g. "20 test functions, all passing"). This recounts the actual
    `void TestXxx(...)` definitions in tests/host/*.cpp and flags any
    documented count that doesn't match -- the same category of check as
    OrcSDR's main.cpp byte/line measurement, applied to test coverage
    instead of file size.
    """
    tests_dir = root / "tests/host"
    if not tests_dir.is_dir():
        return
    actual = 0
    for cpp in sorted(tests_dir.glob("test_*.cpp")):
        actual += len(_TEST_FUNCTION_PATTERN.findall(cpp.read_text(encoding="utf-8")))

    checked_any = False
    for path in _current_markdown(root):
        text = path.read_text(encoding="utf-8")
        for match in _TEST_COUNT_CLAIM_PATTERN.finditer(text):
            checked_any = True
            documented = int(match.group(1))
            if documented != actual:
                report.add(
                    "ERROR", "test-count", path.relative_to(root).as_posix(),
                    _line(text, match.start()),
                    f"documented test-function count ({documented}) does not match "
                    f"actual count in tests/host/test_*.cpp ({actual})",
                )
    if checked_any and not any(item.code == "test-count" for item in report.errors):
        report.passes.append(f"Documented host test-function count matches actual ({actual})")


_COMPONENT_VERSION_PATTERN = re.compile(r"^\s*version\s*:\s*[\"']?([0-9][\w.\-]*)", re.M)


def _check_component_version(root: Path, report: Report) -> None:
    """idf_component.yml's `version:` is the single source of truth for
    OrcMaps' declared component version. Any doc that quotes a specific
    version string (e.g. STATUS.md's "declares version 0.1.0") must quote
    the *current* one -- this mirrors OrcSDR's driver-pin check, adapted
    from an external dependency pin to this repo's own manifest version.
    """
    manifest_path = "idf_component.yml"
    manifest = _text(root, manifest_path)
    if manifest is None:
        report.add("ERROR", "component-version", manifest_path, 1, "idf_component.yml is missing")
        return
    match = _COMPONENT_VERSION_PATTERN.search(manifest)
    if not match:
        report.add("ERROR", "component-version", manifest_path, 1, "no version field found")
        return
    current = match.group(1)

    # Semver only: `\d+\.\d+\.\d+` with an optional `-pre`/`+build` suffix,
    # deliberately NOT swallowing an arbitrary trailing `.` -- otherwise a
    # version mentioned at the end of a sentence ("...declares version
    # 0.1.0.") would capture the sentence's full stop as part of the
    # version string and never match the manifest.
    version_mention = re.compile(
        r"version[^\d\n]{0,12}([0-9]+\.[0-9]+\.[0-9]+(?:[-+][\w.\-]*)?)", re.I
    )
    checked_any = False
    for path in _current_markdown(root):
        text = path.read_text(encoding="utf-8")
        masked = _mask_code_fences(text)
        for m in version_mention.finditer(masked):
            quoted = m.group(1)
            # Only a version-looking token that isn't the current one, and
            # isn't clearly framed as historical/example text, is drift.
            if quoted == current:
                checked_any = True
                continue
            context = text[max(0, m.start() - 60):m.start()]
            if re.search(r"e\.g\.|example|roadmap|milestone|historical", context, re.I):
                continue
            checked_any = True
            report.add(
                "ERROR", "component-version", path.relative_to(root).as_posix(),
                _line(text, m.start()),
                f"quotes component version {quoted}, but idf_component.yml declares {current}",
            )
    if checked_any and not any(item.code == "component-version" for item in report.errors):
        report.passes.append(f"Documented component version matches idf_component.yml ({current})")


_REQUIRED_PROVENANCE_DOCS = (
    "docs/DATA_AND_LICENSING.md",
    "docs/DATA_PROVENANCE_REGISTRY.md",
)


def _check_provenance_docs_exist(root: Path, report: Report) -> None:
    """Documentation Truth stays responsible for doc consistency (not
    license policy itself -- that's tools/check_data_provenance.py), but it
    does verify the provenance *documentation* this project's IP-safety
    principle (PROJECT_TRUTH.md) depends on hasn't quietly disappeared, and
    that PROJECT_TRUTH.md hasn't drifted away from stating that principle.
    """
    for relative in _REQUIRED_PROVENANCE_DOCS:
        if _text(root, relative) is None:
            report.add(
                "ERROR", "provenance-docs-missing", relative,
                1, "required provenance policy document is missing",
            )
    truth = _text(root, "PROJECT_TRUTH.md")
    if truth is not None and not re.search(r"provenance", truth, re.I):
        report.add(
            "ERROR", "provenance-principle-missing", "PROJECT_TRUTH.md", 1,
            "PROJECT_TRUTH.md no longer mentions provenance -- the IP/provenance "
            "safety principle must stay documented there",
        )
    if not any(item.code in ("provenance-docs-missing", "provenance-principle-missing")
               for item in report.errors):
        report.passes.append("Provenance policy documentation is present")


def _check_resolved_claims(root: Path, report: Report) -> None:
    for relative, patterns in RESOLVED_CLAIM_GUARDS.items():
        text = _text(root, relative)
        if text is None:
            continue
        for pattern in patterns:
            match = re.search(pattern, text, re.I)
            if match:
                report.add(
                    "ERROR", "resolved-claim", relative, _line(text, match.start()),
                    "known obsolete current-state claim resurfaced",
                )
    if RESOLVED_CLAIM_GUARDS and not any(item.code == "resolved-claim" for item in report.errors):
        report.passes.append("Resolved high-risk claims remain absent")


def _check_prompt_residue(root: Path, report: Report) -> None:
    for path in _current_markdown(root):
        text = path.read_text(encoding="utf-8")
        for pattern in PROMPT_PATTERNS:
            match = pattern.search(text)
            if match:
                report.add(
                    "WARNING", "prompt-residue", path.relative_to(root).as_posix(),
                    _line(text, match.start()), f"possible prompt residue: {match.group(0)}",
                )


def _check_history_labels(root: Path, report: Report) -> None:
    for relative in HISTORICAL_DOCS:
        text = _text(root, relative)
        if text is not None and not re.search(r"Historical|Superseded", text[:800], re.I):
            report.add(
                "WARNING", "history-label", relative, 1,
                "historical document lacks a visible historical/superseded notice",
            )
    history = root / "docs/history"
    if history.is_dir():
        for path in history.rglob("*.md"):
            text = path.read_text(encoding="utf-8")
            if not re.search(r"Historical|Superseded", text[:800], re.I):
                report.add(
                    "WARNING", "history-label", path.relative_to(root).as_posix(), 1,
                    "docs/history file lacks a visible historical/superseded notice",
                )


def run_checks(root: Path) -> Report:
    root = root.resolve()
    report = Report()
    for check in (
        _check_ci_claims,
        _check_links,
        _check_file_references,
        _check_component_table_dirs,
        _check_test_function_count,
        _check_component_version,
        _check_provenance_docs_exist,
        _check_resolved_claims,
        _check_prompt_residue,
        _check_history_labels,
    ):
        check(root, report)
    return report


def _render(report: Report) -> str:
    lines = ["OrcMaps Documentation Truth Check", "=================================="]
    lines.extend(f"PASS  {message}" for message in report.passes)
    for item in report.warnings:
        lines.append(f"WARN  {item.path}:{item.line}: {item.message}")
    for item in report.errors:
        lines.append(f"ERROR {item.path}:{item.line}: {item.message}")
    lines.extend(("", f"{len(report.errors)} errors", f"{len(report.warnings)} warnings"))
    return "\n".join(lines)


def _github_output(report: Report, rendered: str) -> None:
    for item in report.errors + report.warnings:
        level = "error" if item.level == "ERROR" else "warning"
        print(f"::{level} file={item.path},line={item.line}::{item.message}")
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
