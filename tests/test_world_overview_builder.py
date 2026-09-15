import importlib.util
import json
import re
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).parents[1]
SCRIPT = ROOT / "tools" / "pack-builder" / "build_world_overview.py"
SPEC = importlib.util.spec_from_file_location("world_overview_builder", SCRIPT)
BUILDER = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(BUILDER)


class WorldOverviewBuilderTests(unittest.TestCase):
    def test_profile_has_exact_scales_layers_and_sources(self):
        profile = BUILDER.load_profile(ROOT / "tools/pack-builder/world_overview_profile.json")
        sources = BUILDER.load_json(ROOT / "tools/pack-builder/world_overview_sources.json")
        self.assertEqual(profile["schema_version"], "orcmaps-overview-1")
        self.assertEqual(profile["zoom_ranges"], {
            "110m": {"min": 0, "max": 2},
            "50m": {"min": 3, "max": 5},
            "10m": {"min": 6, "max": 8},
        })
        self.assertEqual(set(profile["layers"]), {"land", "water", "waterway", "boundary", "place"})
        self.assertEqual(len(sources["datasets"]), 21)

    def test_inputs_reject_changed_archive(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / "110m/archives/ne_110m_land.zip"
            dataset = root / "110m/datasets/ne_110m_land"
            archive.parent.mkdir(parents=True)
            dataset.mkdir(parents=True)
            archive.write_bytes(b"wrong")
            for suffix in (".shp", ".shx", ".dbf", ".prj"):
                (dataset / f"ne_110m_land{suffix}").write_bytes(b"x")
            definition = {"collection_version": "5.1.2", "datasets": [{
                "scale": "110m", "dataset": "land", "use": "land",
                "geometry": "polygon", "archive": archive.name,
                "sha256": "0" * 64, "extracted_path": "110m/datasets/ne_110m_land",
            }]}
            with self.assertRaisesRegex(ValueError, "SHA-256"):
                BUILDER.validate_inputs(definition, root)

    def test_commands_are_local_and_build_master_once(self):
        profile = BUILDER.load_profile(ROOT / "tools/pack-builder/world_overview_profile.json")
        sources = BUILDER.load_json(ROOT / "tools/pack-builder/world_overview_sources.json")
        commands = BUILDER.build_commands(
            profile, sources, Path("C:/sources"), Path("C:/out"),
            Path("C:/planetiler.jar"), "java", "javac", Path("C:/pmtiles.exe"),
            ROOT / "tools/pack-builder/WorldOverviewProfile.java")
        text = json.dumps(commands)
        self.assertEqual([c[1] for c in commands if len(c) > 1 and c[1] == "extract"],
                         ["extract", "extract"])
        self.assertEqual(text.count("world-overview-z8.partial.pmtiles"), 1)
        self.assertNotIn("http", text)
        self.assertNotIn("download", text.lower())

    def test_world_pack_identity_is_deterministic(self):
        self.assertEqual(
            BUILDER.pack_id(7),
            "5.1.2__orcmaps-overview-1__overview__world"
            "__b-1800000000_-850511288_1800000000_850511288__z0-7")

    def test_sidecar_records_local_inputs_and_derivation(self):
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory) / "world-overview-z7.pmtiles"
            archive.write_bytes(b"test pack")
            BUILDER.write_sidecars(archive, 7, {"ne.zip": "a" * 64},
                                   {"go_pmtiles": "pmtiles 1.28.2"},
                                   [["pmtiles", "extract"]], "world-overview-z8.pmtiles")
            manifest = json.loads(archive.with_suffix(".manifest.json").read_text())
            self.assertEqual(manifest["pack_id"], BUILDER.pack_id(7))
            self.assertEqual(manifest["schema_version"], "orcmaps-overview-1")
            self.assertEqual(manifest["derived_from"], "world-overview-z8.pmtiles")
            self.assertEqual(manifest["input_hashes"], {"ne.zip": "a" * 64})
            self.assertEqual(archive.with_suffix(".sha256").read_text().split()[0],
                             BUILDER.sha256(archive))


class WorldOverviewBoundsTests(unittest.TestCase):
    """The emitted bounds must be loadable by the engine, not merely close."""

    def engine_mercator_limit(self):
        text = (Path(BUILDER.__file__).parents[2] / "include" / "orcmap"
                / "geo.hpp").read_text(encoding="utf-8")
        match = re.search(r"kMercatorMaxLatDeg\s*=\s*([0-9.]+)", text)
        self.assertIsNotNone(match, "kMercatorMaxLatDeg not found in geo.hpp")
        return float(match.group(1))

    def test_builder_constant_matches_the_engine_header(self):
        self.assertEqual(self.engine_mercator_limit(),
                         BUILDER.MERCATOR_MAX_LAT_DEG)

    def test_emitted_bounds_are_within_the_engine_limit(self):
        # The original defect: 85.0511288 > 85.05112878, so ValidBounds() --
        # and therefore runtime SD discovery -- rejected every manifest this
        # builder wrote. Rounding a bound outward is never safe.
        limit = self.engine_mercator_limit()
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory) / "world-overview-z7.pmtiles"
            archive.write_bytes(b"test pack")
            BUILDER.write_sidecars(archive, 7, {"ne.zip": "a" * 64},
                                   {"go_pmtiles": "pmtiles 1.28.2"},
                                   [["pmtiles", "extract"]], None)
            bounds = json.loads(
                archive.with_suffix(".manifest.json").read_text())["bounds"]
        self.assertLessEqual(abs(bounds["min_lat"]), limit)
        self.assertLessEqual(abs(bounds["max_lat"]), limit)
        self.assertEqual(bounds["min_lat"], -limit)
        self.assertEqual(bounds["max_lat"], limit)

    def test_identity_is_unchanged_by_the_fix(self):
        # pack_id embeds llround(lat * 1e7); both the old rounded value and
        # the exact constant yield 850511288, so fixing the bound does not
        # rename existing packs.
        self.assertEqual(round(85.0511288 * 1e7),
                         round(BUILDER.MERCATOR_MAX_LAT_DEG * 1e7))


if __name__ == "__main__":
    unittest.main()
