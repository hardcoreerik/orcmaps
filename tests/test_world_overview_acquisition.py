import hashlib
import importlib.util
import io
import json
from pathlib import Path
import struct
import tempfile
import unittest
import zipfile


ROOT = Path(__file__).parents[1]
SCRIPT = ROOT / "tools" / "pack-builder" / "acquire_world_overview_sources.py"
DEFINITION = ROOT / "tools" / "pack-builder" / "world_overview_sources.json"
SPEC = importlib.util.spec_from_file_location("world_overview_acquisition", SCRIPT)
ACQUIRE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(ACQUIRE)


def archive_bytes(*, include_prj=True):
    output = io.BytesIO()
    stem = "ne_110m_land"
    with zipfile.ZipFile(output, "w") as archive:
        dbf = bytearray(8)
        dbf[4:8] = struct.pack("<I", 7)
        shp = bytearray(36)
        shp[32:36] = struct.pack("<I", 5)
        archive.writestr(stem + ".dbf", dbf)
        archive.writestr(stem + ".shp", shp)
        archive.writestr(stem + ".shx", b"index")
        if include_prj:
            archive.writestr(stem + ".prj", "WGS84")
    return output.getvalue()


def definition(payload):
    return {
        "schema_version": 1,
        "provider": "Natural Earth",
        "collection_version": "5.1.2",
        "release_date": "2022-05-13",
        "official_release_url": "https://github.com/nvkelso/natural-earth-vector/releases/tag/v5.1.2",
        "official_base_url": "https://naturalearth.s3.amazonaws.com/",
        "provenance_id": "natural-earth",
        "datasets": [{
            "scale": "110m", "category": "physical", "dataset": "land",
            "theme_version": "4.0.0", "geometry": "polygon", "use": "land",
            "url": "https://naturalearth.s3.amazonaws.com/110m_physical/ne_110m_land.zip",
            "archive": "ne_110m_land.zip", "expected_bytes": len(payload),
            "sha256": hashlib.sha256(payload).hexdigest(),
        }],
    }


class WorldOverviewAcquisitionTests(unittest.TestCase):
    def test_pinned_definition_has_all_seven_layers_at_three_scales(self):
        source = ACQUIRE.load_definition(DEFINITION)
        self.assertEqual(len(source["datasets"]), 21)
        self.assertEqual({item["scale"] for item in source["datasets"]}, ACQUIRE.SCALES)
        self.assertEqual(
            {item["dataset"] for item in source["datasets"] if item["scale"] == "10m"},
            {"land", "ocean", "lakes", "rivers_lake_centerlines",
             "admin_0_boundary_lines_land", "admin_1_states_provinces_lines",
             "populated_places_simple"},
        )

    def test_definition_rejects_nonofficial_and_duplicate_sources(self):
        payload = archive_bytes()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sources.json"
            bad = definition(payload)
            bad["datasets"][0]["url"] = "https://example.com/ne_110m_land.zip"
            path.write_text(json.dumps(bad), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "non-official"):
                ACQUIRE.load_definition(path)
            duplicate = definition(payload)
            duplicate["datasets"].append(duplicate["datasets"][0].copy())
            path.write_text(json.dumps(duplicate), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "duplicate"):
                ACQUIRE.load_definition(path)

    def test_acquires_layout_checksum_inventory_and_reuses_archive(self):
        payload = archive_bytes()
        calls = []

        def opener(url):
            calls.append(url)
            return io.BytesIO(payload)

        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory)
            record = ACQUIRE.acquire(definition(payload), destination, opener=opener)
            root = destination / "5.1.2"
            self.assertEqual(record["datasets"][0]["feature_count"], 7)
            self.assertEqual(record["datasets"][0]["geometry"], "polygon")
            self.assertTrue((root / "110m/archives/ne_110m_land.zip").is_file())
            self.assertTrue((root / "110m/datasets/ne_110m_land/ne_110m_land.shp").is_file())
            saved = json.loads((root / "SOURCE.json").read_text(encoding="utf-8"))
            self.assertEqual(saved["datasets"][0]["sha256"], hashlib.sha256(payload).hexdigest())
            self.assertIn("110m/archives/ne_110m_land.zip", (root / "SHA256SUMS.txt").read_text())
            ACQUIRE.acquire(definition(payload), destination, opener=opener)
            self.assertEqual(len(calls), 1)

    def test_refuses_to_overwrite_invalid_existing_archive(self):
        payload = archive_bytes()
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory)
            archive = destination / "5.1.2/110m/archives/ne_110m_land.zip"
            archive.parent.mkdir(parents=True)
            archive.write_bytes(b"wrong")
            with self.assertRaisesRegex(FileExistsError, "refusing to overwrite"):
                ACQUIRE.acquire(definition(payload), destination,
                                opener=lambda _url: io.BytesIO(payload))

    def test_rejects_archive_missing_required_component(self):
        payload = archive_bytes(include_prj=False)
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "missing: .prj"):
                ACQUIRE.acquire(definition(payload), Path(directory),
                                opener=lambda _url: io.BytesIO(payload))


if __name__ == "__main__":
    unittest.main()
