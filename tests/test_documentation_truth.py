import tempfile
import unittest
from pathlib import Path

from tools.check_documentation_truth import run_checks


class DocumentationTruthTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.write("idf_component.yml", 'version: "0.1.0"\n')
        self.write(".github/workflows/core.yml", "name: Core\n")
        self.write(
            "tests/host/test_geo.cpp",
            "void TestOne() {}\nvoid TestTwo() {}\n",
        )
        self.write("src/core/geo.cpp", "// stub for file-reference resolution\n")
        self.write("README.md", "OrcMaps.\n")
        self.write(
            "STATUS.md",
            "Host test suite: 2 test functions, all passing.\n"
            "Declares version 0.1.0.\n",
        )
        self.write("PROJECT_TRUTH.md", "Project truth. Uses an IP/provenance safety model.\n")
        self.write("docs/DATA_AND_LICENSING.md", "Data and licensing policy.\n")
        self.write("docs/DATA_PROVENANCE_REGISTRY.md", "Provenance registry schema.\n")
        self.write("CONTRIBUTING.md", "Contributing guide.\n")
        self.write(
            "ARCHITECTURE.md",
            "| Component | Path | Status |\n"
            "|---|---|---|\n"
            "| Empty thing | `adapters/m5gfx/` | Not implemented (empty dir) |\n"
            "| Core thing | `src/core/geo.cpp` | Implemented |\n",
        )
        self.write("ROADMAP.md", "Roadmap.\n")
        self.write("LICENSING.md", "AGPL-3.0.\n")

    def tearDown(self):
        self.temp.cleanup()

    def write(self, relative, text):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def report(self):
        return run_checks(self.root)

    def test_correct_current_state_passes(self):
        self.assertEqual([], self.report().errors)

    def test_unqualified_no_ci_claim_fails_when_workflows_exist(self):
        self.write("README.md", "OrcMaps has no CI.\n")
        self.assertTrue(any(item.code == "no-ci" for item in self.report().errors))

    def test_broken_local_markdown_link_fails(self):
        self.write("README.md", "[Missing](docs/missing.md)\n")
        self.assertTrue(any(item.code == "local-link" for item in self.report().errors))

    def test_valid_local_markdown_link_passes(self):
        self.write("README.md", "[Roadmap](ROADMAP.md)\n")
        self.assertEqual([], self.report().errors)

    def test_broken_file_reference_fails(self):
        self.write("README.md", "See `src/core/does_not_exist.cpp`.\n")
        self.assertTrue(any(item.code == "file-reference" for item in self.report().errors))

    def test_brace_expansion_reference_is_not_a_false_positive(self):
        # `foo.{hpp,cpp}` is shorthand for two files, not a literal path --
        # must not be existence-checked as one.
        self.write("README.md", "See `src/core/geo.{hpp,cpp}`.\n")
        self.assertFalse(any(item.code == "file-reference" for item in self.report().errors))

    def test_empty_dir_claim_with_actual_files_fails(self):
        self.write("adapters/m5gfx/renderer.cpp", "// not actually empty\n")
        self.assertTrue(any(item.code == "component-table-drift" for item in self.report().errors))

    def test_empty_dir_claim_matching_reality_passes(self):
        self.assertEqual([], self.report().errors)

    def test_implemented_dir_claim_without_files_fails(self):
        text = (self.root / "ARCHITECTURE.md").read_text(encoding="utf-8")
        text += "| Host tests | `tests/host/` | Implemented, 100% passing |\n"
        self.write("ARCHITECTURE.md", text)
        (self.root / "tests/host/test_geo.cpp").unlink()
        self.assertTrue(any(item.code == "component-table-drift" for item in self.report().errors))

    def test_stale_test_count_claim_fails(self):
        self.write(
            "STATUS.md",
            "Host test suite: 99 test functions, all passing.\nDeclares version 0.1.0.\n",
        )
        self.assertTrue(any(item.code == "test-count" for item in self.report().errors))

    def test_correct_test_count_claim_passes(self):
        self.assertEqual([], self.report().errors)

    def test_stale_component_version_claim_fails(self):
        self.write(
            "STATUS.md",
            "Host test suite: 2 test functions, all passing.\n"
            "Declares version 0.2.0.\n",
        )
        self.assertTrue(any(item.code == "component-version" for item in self.report().errors))

    def test_prompt_residue_warns_without_failing(self):
        self.write("README.md", "Your job is to implement the following.\n")
        report = self.report()
        self.assertEqual([], report.errors)
        self.assertTrue(any(item.code == "prompt-residue" for item in report.warnings))

    def test_history_doc_without_label_warns(self):
        self.write("docs/history/old.md", "Some stale note with no marker.\n")
        report = self.report()
        self.assertTrue(any(item.code == "history-label" for item in report.warnings))

    def test_history_doc_with_label_passes(self):
        self.write("docs/history/old.md", "Historical: some stale note.\n")
        report = self.report()
        self.assertFalse(any(item.code == "history-label" for item in report.warnings))

    def test_missing_provenance_doc_fails(self):
        (self.root / "docs/DATA_AND_LICENSING.md").unlink()
        self.assertTrue(
            any(item.code == "provenance-docs-missing" for item in self.report().errors)
        )

    def test_project_truth_without_provenance_mention_fails(self):
        self.write("PROJECT_TRUTH.md", "Project truth, no relevant mention here.\n")
        self.assertTrue(
            any(item.code == "provenance-principle-missing" for item in self.report().errors)
        )

    def test_resolved_m5gfx_retarget_claim_fails(self):
        self.write("README.md", "The M5GFX retarget is not yet built.\n")
        self.assertTrue(
            any(item.code == "resolved-claim" for item in self.report().errors)
        )

    def test_m5gfx_example_relative_include_hack_fails(self):
        self.write(
            "examples/m5gfx/main/CMakeLists.txt",
            'idf_component_register(INCLUDE_DIRS "../../../adapters/m5gfx")\n',
        )
        self.assertTrue(
            any(item.code == "resolved-claim" for item in self.report().errors)
        )


if __name__ == "__main__":
    unittest.main()
