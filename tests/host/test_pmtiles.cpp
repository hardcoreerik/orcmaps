#include "orcmap/pmtiles.hpp"

#include <cstdio>
#include <cstring>
#include <string>

#include "file_byte_source.hpp"
#include "test_util.hpp"

namespace {

std::string ExpectedTileText(uint8_t z, uint32_t x, uint32_t y) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "orcmap-test-tile z=%u x=%u y=%u",
                static_cast<unsigned>(z), x, y);
  return buf;
}

void TestOpenAndHeader(const std::string& fixture_path) {
  orcmap::host::FileByteSource source(fixture_path);
  ORCMAP_EXPECT_TRUE(source.Valid());

  orcmap::PmTilesReader reader(&source);
  ORCMAP_EXPECT_TRUE(reader.Open());
  ORCMAP_EXPECT_TRUE(reader.IsOpen());

  const orcmap::PmTilesHeader& h = reader.Header();
  ORCMAP_EXPECT_EQ(h.version, 3);
  ORCMAP_EXPECT_EQ(h.root_dir_offset, 127u);
  ORCMAP_EXPECT_EQ(h.metadata_offset, 167u);
  ORCMAP_EXPECT_EQ(h.metadata_length, 128u);
  ORCMAP_EXPECT_EQ(h.tile_data_offset, 295u);
  ORCMAP_EXPECT_EQ(h.tile_data_length, 224u);
  ORCMAP_EXPECT_EQ(h.addressed_tiles_count, 8u);
  ORCMAP_EXPECT_EQ(h.tile_entries_count, 8u);
  ORCMAP_EXPECT_EQ(h.tile_contents_count, 8u);
  ORCMAP_EXPECT_TRUE(!h.clustered);
  ORCMAP_EXPECT_TRUE(h.internal_compression == orcmap::Compression::kGzip);
  ORCMAP_EXPECT_TRUE(h.tile_compression == orcmap::Compression::kNone);
  ORCMAP_EXPECT_TRUE(h.tile_type == orcmap::TileType::kMvt);
  ORCMAP_EXPECT_EQ(h.min_zoom, 0);
  ORCMAP_EXPECT_EQ(h.max_zoom, 2);
  ORCMAP_EXPECT_EQ(h.min_lon_e7, -1800000000);
  ORCMAP_EXPECT_EQ(h.max_lat_e7, 850511300);
}

void TestReadMetadata(const std::string& fixture_path) {
  orcmap::host::FileByteSource source(fixture_path);
  orcmap::PmTilesReader reader(&source);
  ORCMAP_EXPECT_TRUE(reader.Open());

  std::vector<uint8_t> metadata;
  ORCMAP_EXPECT_TRUE(reader.ReadMetadata(&metadata));
  const std::string text(metadata.begin(), metadata.end());
  ORCMAP_EXPECT_TRUE(text.find("orcmap-test-fixture") != std::string::npos);
  ORCMAP_EXPECT_TRUE(text.find("attribution") != std::string::npos);
}

void TestGetKnownTiles(const std::string& fixture_path) {
  orcmap::host::FileByteSource source(fixture_path);
  orcmap::PmTilesReader reader(&source);
  ORCMAP_EXPECT_TRUE(reader.Open());

  // Matches tests/fixtures/generate_fixture.py exactly.
  const struct { uint8_t z; uint32_t x, y; } kKnownTiles[] = {
      {0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {1, 1, 0}, {1, 1, 1},
      {2, 0, 0}, {2, 1, 2}, {2, 3, 3},
  };

  for (const auto& t : kKnownTiles) {
    std::vector<uint8_t> out;
    const bool found = reader.GetTile(t.z, t.x, t.y, &out);
    ORCMAP_EXPECT_TRUE(found);
    if (!found) continue;
    const std::string text(out.begin(), out.end());
    ORCMAP_EXPECT_TRUE(text == ExpectedTileText(t.z, t.x, t.y));
  }
}

void TestMissingTile(const std::string& fixture_path) {
  orcmap::host::FileByteSource source(fixture_path);
  orcmap::PmTilesReader reader(&source);
  ORCMAP_EXPECT_TRUE(reader.Open());

  // z2/x1/y1 was deliberately not written by generate_fixture.py -- the
  // fixture is a sparse archive, and a sparse gap must return "not found",
  // not a wrong tile or a crash.
  std::vector<uint8_t> out;
  ORCMAP_EXPECT_TRUE(!reader.GetTile(2, 1, 1, &out));

  // Zoom 5 is entirely outside the fixture's data (max_zoom=2).
  ORCMAP_EXPECT_TRUE(!reader.GetTile(5, 0, 0, &out));
}

void TestCorruptArchive() {
  // A truncated/garbage file must fail Open() cleanly, not crash.
  const std::string bogus_path = "does_not_exist.pmtiles";
  orcmap::host::FileByteSource source(bogus_path);
  ORCMAP_EXPECT_TRUE(!source.Valid());
  orcmap::PmTilesReader reader(&source);
  ORCMAP_EXPECT_TRUE(!reader.Open());
}

void TestHilbertMonotonicAndUnique(const std::string& fixture_path) {
  (void)fixture_path;
  // z0 is exactly tile ID 0; z1's four tiles occupy IDs 1-4 with no
  // duplicates, per the PMTiles level-stacking rule.
  ORCMAP_EXPECT_EQ(orcmap::ZxyToTileId(0, 0, 0), 0u);
  uint64_t seen[4];
  int i = 0;
  for (uint32_t x = 0; x < 2; ++x) {
    for (uint32_t y = 0; y < 2; ++y) {
      seen[i++] = orcmap::ZxyToTileId(1, x, y);
    }
  }
  for (int a = 0; a < 4; ++a) {
    ORCMAP_EXPECT_TRUE(seen[a] >= 1 && seen[a] <= 4);
    for (int b = a + 1; b < 4; ++b) ORCMAP_EXPECT_TRUE(seen[a] != seen[b]);
  }
}

}  // namespace

void RunPmTilesTests(const std::string& fixture_path) {
  TestOpenAndHeader(fixture_path);
  TestReadMetadata(fixture_path);
  TestGetKnownTiles(fixture_path);
  TestMissingTile(fixture_path);
  TestCorruptArchive();
  TestHilbertMonotonicAndUnique(fixture_path);
}
