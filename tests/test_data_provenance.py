import json
import tempfile
import unittest
from pathlib import Path

from tools.check_data_provenance import run_checks


def valid_record(**overrides):
    record = {
        "id": "example-source",
        "name": "Example Source",
        "publisher": "Example Publisher",
        "source": {"url": "https://example.com/data", "retrieved": "2026-09-13"},
        "rights": {
            "license": "Public Domain",
            "license_class": "PUBLIC_DOMAIN_VERIFIED",
            "commercial_use_allowed": True,
            "redistribution_allowed": True,
            "modification_allowed": True,
            "attribution_required": False,
            "share_alike_required": False,
            "source_disclosure_required": False,
        },
        "evidence": {
            "rights_url": "https://example.com/rights",
            "reviewed_date": "2026-09-13",
            "confidence": "CONFIRMED",
        },
        "policy": {"official_pack_allowed": True, "clean_pack_allowed": True},
    }
    for key, value in overrides.items():
        record[key] = value
    return record


class DataProvenanceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def write_record(self, filename, record):
        path = self.root / "data" / "sources" / filename
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(record, indent=2), encoding="utf-8")

    def report(self):
        return run_checks(self.root)

    def test_valid_record_passes(self):
        self.write_record("example-source.json", valid_record())
        self.assertEqual([], self.report().errors)

    def test_missing_registry_dir_fails(self):
        report = self.report()
        self.assertTrue(any(item.code == "no-registry" for item in report.errors))

    def test_missing_required_field_fails(self):
        record = valid_record()
        del record["evidence"]["reviewed_date"]
        self.write_record("example-source.json", record)
        self.assertTrue(any(item.code == "missing-field" for item in self.report().errors))

    def test_id_filename_mismatch_fails(self):
        self.write_record("wrong-name.json", valid_record())
        self.assertTrue(any(item.code == "id-filename-mismatch" for item in self.report().errors))

    def test_duplicate_id_fails(self):
        self.write_record("example-source.json", valid_record())
        self.write_record("example-source-copy.json", valid_record(id="example-source"))
        self.assertTrue(any(item.code == "duplicate-id" for item in self.report().errors))

    def test_invalid_json_fails(self):
        path = self.root / "data" / "sources" / "broken.json"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("{not valid json", encoding="utf-8")
        self.assertTrue(any(item.code == "invalid-json" for item in self.report().errors))

    def test_unknown_license_class_fails(self):
        record = valid_record()
        record["rights"]["license_class"] = "MADE_UP_CLASS"
        self.write_record("example-source.json", record)
        self.assertTrue(any(item.code == "unknown-license-class" for item in self.report().errors))

    def test_odbl_without_share_alike_fails(self):
        record = valid_record(id="osm-like")
        record["rights"]["license_class"] = "ODBL"
        record["rights"]["share_alike_required"] = False
        record["policy"] = {"official_pack_allowed": True, "clean_pack_allowed": False}
        self.write_record("osm-like.json", record)
        self.assertTrue(any(item.code == "license-class-mismatch" for item in self.report().errors))

    def test_odbl_with_correct_fields_passes(self):
        record = valid_record(id="osm-like")
        record["rights"]["license_class"] = "ODBL"
        record["rights"]["share_alike_required"] = True
        record["rights"]["attribution_required"] = True
        record["policy"] = {"official_pack_allowed": True, "clean_pack_allowed": False}
        self.write_record("osm-like.json", record)
        self.assertEqual([], self.report().errors)

    def test_odbl_cannot_be_clean_pack_allowed(self):
        record = valid_record(id="osm-like")
        record["rights"]["license_class"] = "ODBL"
        record["rights"]["share_alike_required"] = True
        record["rights"]["attribution_required"] = True
        record["policy"] = {"official_pack_allowed": True, "clean_pack_allowed": True}
        self.write_record("osm-like.json", record)
        self.assertTrue(any(item.code == "clean-pack-not-eligible" for item in self.report().errors))

    def test_permissive_attribution_cannot_be_clean_pack_allowed(self):
        record = valid_record(id="permissive-like")
        record["rights"]["license_class"] = "PERMISSIVE_ATTRIBUTION"
        record["rights"]["attribution_required"] = True
        record["policy"] = {"official_pack_allowed": True, "clean_pack_allowed": True}
        self.write_record("permissive-like.json", record)
        self.assertTrue(any(item.code == "clean-pack-not-eligible" for item in self.report().errors))

    def test_unknown_cannot_be_official(self):
        record = valid_record(id="mystery-source")
        record["rights"]["license_class"] = "UNKNOWN"
        record["policy"] = {"official_pack_allowed": True, "clean_pack_allowed": False}
        self.write_record("mystery-source.json", record)
        self.assertTrue(any(item.code == "unknown-not-official" for item in self.report().errors))

    def test_noncommercial_cannot_be_official(self):
        record = valid_record(id="nc-source")
        record["rights"]["license_class"] = "NONCOMMERCIAL"
        record["rights"]["commercial_use_allowed"] = False
        record["policy"] = {"official_pack_allowed": True, "clean_pack_allowed": False}
        self.write_record("nc-source.json", record)
        self.assertTrue(any(item.code == "noncommercial-not-official" for item in self.report().errors))

    def test_review_required_forces_official_false(self):
        record = valid_record()
        record["evidence"]["confidence"] = "REVIEW_REQUIRED"
        record["policy"] = {"official_pack_allowed": True, "clean_pack_allowed": False}
        self.write_record("example-source.json", record)
        self.assertTrue(any(item.code == "review-required-not-official" for item in self.report().errors))

    def test_review_required_forces_clean_false(self):
        record = valid_record()
        record["evidence"]["confidence"] = "REVIEW_REQUIRED"
        record["policy"] = {"official_pack_allowed": False, "clean_pack_allowed": True}
        self.write_record("example-source.json", record)
        self.assertTrue(any(item.code == "review-required-not-clean" for item in self.report().errors))

    def test_review_required_with_both_false_passes(self):
        record = valid_record()
        record["evidence"]["confidence"] = "REVIEW_REQUIRED"
        record["policy"] = {"official_pack_allowed": False, "clean_pack_allowed": False}
        self.write_record("example-source.json", record)
        self.assertEqual([], self.report().errors)

    def test_bad_url_fails(self):
        record = valid_record()
        record["source"]["url"] = "not-a-url"
        self.write_record("example-source.json", record)
        self.assertTrue(any(item.code == "bad-url" for item in self.report().errors))

    def test_bad_date_fails(self):
        record = valid_record()
        record["evidence"]["reviewed_date"] = "Sept 13 2026"
        self.write_record("example-source.json", record)
        self.assertTrue(any(item.code == "bad-date" for item in self.report().errors))

    def test_no_derivatives_forces_modification_false(self):
        record = valid_record(id="static-source")
        record["rights"]["license_class"] = "NO_DERIVATIVES"
        record["rights"]["modification_allowed"] = True
        record["policy"] = {"official_pack_allowed": False, "clean_pack_allowed": False}
        self.write_record("static-source.json", record)
        self.assertTrue(any(item.code == "license-class-mismatch" for item in self.report().errors))

    def test_real_registry_is_valid(self):
        # The actual OrcMaps registry (data/sources/ at the repo root this
        # test file lives in) must always pass -- this is the same "test
        # against real repo state" discipline as the documentation-truth
        # checker's own suite.
        real_root = Path(__file__).resolve().parents[1]
        report = run_checks(real_root)
        self.assertEqual([], report.errors, msg=str(report.errors))


if __name__ == "__main__":
    unittest.main()
