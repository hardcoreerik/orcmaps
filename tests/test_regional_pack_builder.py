import argparse
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


SCRIPT = Path(__file__).parents[1] / "tools" / "pack-builder" / "build_regional_pack.py"
SPEC = importlib.util.spec_from_file_location("regional_pack_builder", SCRIPT)
BUILDER = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(BUILDER)


class RegionalPackBuilderTests(unittest.TestCase):
    def test_pack_id_matches_cpp_normalization(self):
        args = argparse.Namespace(source_snapshot="2026-09", schema_version="openmaptiles-3.16",
                                  content_profile="Standard", region_id="US-OR",
                                  min_zoom=0, max_zoom=14)
        self.assertEqual(
            BUILDER.pack_id(args, (-124.6, 41.9, -116.4, 46.3)),
            "2026-09__openmaptiles-3.16__standard__us-or__b-1246000000_419000000_-1164000000_463000000__z0-14")

    def test_geojson_bounds_and_invalid_bbox(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "region.geojson"
            path.write_text(json.dumps({"type": "Polygon", "coordinates":
                [[[-123.1, 44.0], [-122.9, 44.0], [-122.9, 44.2], [-123.1, 44.0]]]}))
            self.assertEqual(BUILDER.geojson_bounds(path), (-123.1, 44.0, -122.9, 44.2))
        with self.assertRaises(argparse.ArgumentTypeError):
            BUILDER.parse_bbox("0,-90,1,2")

    def test_sidecars_hash_the_finished_archive(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "oregon.pmtiles"
            output.write_bytes(b"PMTiles test artifact")
            args = argparse.Namespace(
                output=output, pack_version="1", display_name="Oregon", region_id="US-OR",
                region_name="Oregon, USA", min_zoom=0, max_zoom=14,
                content_profile="standard", schema_version="openmaptiles-3.16",
                source_snapshot="2026-09", builder="go-pmtiles extract",
                builder_version="1.28.2",
                builder_commit="a" * 40, build_date="2026-09-14",
                provenance_id="openstreetmap", acquired="2026-09-14",
                source_version="test", pack_class="open",
                attribution=["© OpenStreetMap contributors"], attribution_link=[],
                priority=10, source_label="planet.pmtiles", source_sha256="b" * 64)
            BUILDER.write_sidecars(args, (-124.6, 41.9, -116.4, 46.3))
            manifest = json.loads((Path(directory) / "oregon.manifest.json").read_text())
            checksum = (Path(directory) / "oregon.sha256").read_text().split()[0]
            self.assertEqual(manifest["output_sha256"], checksum)
            self.assertEqual(manifest["size_bytes"], output.stat().st_size)

    def test_openmaptiles_schema_adds_its_own_credit(self):
        # A caller who supplies only the data credit must still get a
        # manifest that satisfies the schema's obligation, because the
        # builder -- not the caller -- knows which schema it emitted.
        args = argparse.Namespace(
            schema_version="openmaptiles-3.16", provenance_id="openstreetmap",
            acquired="2026-09-14", source_version="test",
            attribution=["© OpenStreetMap contributors"],
            attribution_link=["https://www.openstreetmap.org/copyright"])
        sources = BUILDER.apply_schema_attribution(args)
        self.assertEqual(["openstreetmap", "openmaptiles"],
                         [s["provenance_id"] for s in sources])
        # Tile-production credit leads, data credit follows.
        self.assertEqual("© OpenMapTiles", args.attribution[0])
        self.assertIn("© OpenStreetMap contributors", args.attribution)
        self.assertEqual("https://openmaptiles.org/", args.attribution_link[0])

    def test_openmaptiles_credit_is_not_duplicated(self):
        args = argparse.Namespace(
            schema_version="openmaptiles-3.16", provenance_id="openstreetmap",
            acquired="2026-09-14", source_version="test",
            attribution=["© OpenMapTiles",
                         "© OpenStreetMap contributors"],
            attribution_link=["https://openmaptiles.org/"])
        BUILDER.apply_schema_attribution(args)
        self.assertEqual(1, sum(1 for c in args.attribution
                                if "OpenMapTiles" in c))
        self.assertEqual(1, args.attribution_link.count("https://openmaptiles.org/"))

    def test_orcmaps_overview_schema_gets_no_openmaptiles_credit(self):
        args = argparse.Namespace(
            schema_version="orcmaps-overview-1", provenance_id="natural-earth",
            acquired="2026-09-14", source_version="5.1.2",
            attribution=[], attribution_link=[])
        sources = BUILDER.apply_schema_attribution(args)
        self.assertEqual(["natural-earth"], [s["provenance_id"] for s in sources])
        self.assertEqual([], args.attribution)
        self.assertEqual([], args.attribution_link)

    def test_provenance_policy_is_enforced(self):
        args = argparse.Namespace(provenance_id="openstreetmap", pack_class="open",
                                  attribution=["© OpenStreetMap contributors"])
        BUILDER.validate_provenance(args)
        args.pack_class = "clean"
        with self.assertRaises(ValueError):
            BUILDER.validate_provenance(args)
        args.pack_class = "open"
        args.attribution = []
        with self.assertRaises(ValueError):
            BUILDER.validate_provenance(args)


if __name__ == "__main__":
    unittest.main()
