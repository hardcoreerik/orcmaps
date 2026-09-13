#include "orcmap/mvt.hpp"

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

#include "test_util.hpp"

namespace {

std::vector<uint8_t> ReadFile(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),
                               std::istreambuf_iterator<char>());
}

// Numeric attribute values may decode as int64_t, uint64_t, or double
// depending on which MVT wire representation the encoder chose for an
// equal mathematical value -- these tests care about the value, not which
// alternative holds it (string/bool ARE checked exactly, since MVT is
// unambiguous about those two).
double AsNumber(const orcmap::MvtValue& v, bool* ok) {
  *ok = true;
  if (std::holds_alternative<double>(v)) return std::get<double>(v);
  if (std::holds_alternative<int64_t>(v)) return static_cast<double>(std::get<int64_t>(v));
  if (std::holds_alternative<uint64_t>(v)) return static_cast<double>(std::get<uint64_t>(v));
  *ok = false;
  return 0.0;
}

const orcmap::MvtLayer* FindLayer(const orcmap::MvtTile& tile, const std::string& name) {
  for (const auto& layer : tile.layers) {
    if (layer.name == name) return &layer;
  }
  return nullptr;
}

const orcmap::MvtValue* FindAttribute(const orcmap::MvtFeature& feature,
                                       const std::string& key) {
  for (size_t i = 0; i < feature.attribute_keys.size(); ++i) {
    if (feature.attribute_keys[i] == key) return &feature.attribute_values[i];
  }
  return nullptr;
}

void TestDecodeFixture(const std::string& fixture_path) {
  const std::vector<uint8_t> data = ReadFile(fixture_path);
  ORCMAP_EXPECT_TRUE(!data.empty());

  orcmap::MvtTile tile;
  const bool ok = orcmap::DecodeMvtTile(data.data(), data.size(), &tile);
  ORCMAP_EXPECT_TRUE(ok);
  ORCMAP_EXPECT_EQ(tile.layers.size(), static_cast<size_t>(3));

  // --- water: one polygon feature -----------------------------------
  const orcmap::MvtLayer* water = FindLayer(tile, "water");
  ORCMAP_EXPECT_TRUE(water != nullptr);
  if (water == nullptr) return;
  ORCMAP_EXPECT_EQ(water->extent, 4096u);
  ORCMAP_EXPECT_EQ(water->features.size(), static_cast<size_t>(1));
  const orcmap::MvtFeature& lake = water->features[0];
  ORCMAP_EXPECT_TRUE(lake.geom_type == orcmap::MvtGeomType::kPolygon);
  ORCMAP_EXPECT_EQ(lake.geometry.size(), static_cast<size_t>(1));
  ORCMAP_EXPECT_EQ(lake.geometry[0].size(), static_cast<size_t>(4));
  // Ring is a closed square (exact coordinates depend on the encoder's Y
  // flip, but must form a 4x4 square).
  int32_t min_x = lake.geometry[0][0].x, max_x = min_x;
  int32_t min_y = lake.geometry[0][0].y, max_y = min_y;
  for (const auto& p : lake.geometry[0]) {
    min_x = std::min(min_x, p.x);
    max_x = std::max(max_x, p.x);
    min_y = std::min(min_y, p.y);
    max_y = std::max(max_y, p.y);
  }
  ORCMAP_EXPECT_EQ(max_x - min_x, 4);
  ORCMAP_EXPECT_EQ(max_y - min_y, 4);

  const orcmap::MvtValue* kind = FindAttribute(lake, "kind");
  ORCMAP_EXPECT_TRUE(kind != nullptr && std::holds_alternative<std::string>(*kind));
  if (kind != nullptr && std::holds_alternative<std::string>(*kind)) {
    ORCMAP_EXPECT_TRUE(std::get<std::string>(*kind) == "lake");
  }
  const orcmap::MvtValue* area = FindAttribute(lake, "area");
  ORCMAP_EXPECT_TRUE(area != nullptr);
  if (area != nullptr) {
    bool numeric_ok = false;
    ORCMAP_EXPECT_NEAR(AsNumber(*area, &numeric_ok), 12.5, 1e-6);
    ORCMAP_EXPECT_TRUE(numeric_ok);
  }

  // --- road: one linestring feature -----------------------------------
  const orcmap::MvtLayer* road = FindLayer(tile, "road");
  ORCMAP_EXPECT_TRUE(road != nullptr);
  if (road == nullptr) return;
  ORCMAP_EXPECT_EQ(road->features.size(), static_cast<size_t>(1));
  const orcmap::MvtFeature& primary = road->features[0];
  ORCMAP_EXPECT_TRUE(primary.geom_type == orcmap::MvtGeomType::kLineString);
  ORCMAP_EXPECT_EQ(primary.geometry.size(), static_cast<size_t>(1));
  ORCMAP_EXPECT_EQ(primary.geometry[0].size(), static_cast<size_t>(3));

  const orcmap::MvtValue* oneway = FindAttribute(primary, "oneway");
  ORCMAP_EXPECT_TRUE(oneway != nullptr && std::holds_alternative<bool>(*oneway));
  if (oneway != nullptr && std::holds_alternative<bool>(*oneway)) {
    ORCMAP_EXPECT_TRUE(std::get<bool>(*oneway) == true);
  }
  const orcmap::MvtValue* lanes = FindAttribute(primary, "lanes");
  ORCMAP_EXPECT_TRUE(lanes != nullptr);
  if (lanes != nullptr) {
    bool numeric_ok = false;
    ORCMAP_EXPECT_NEAR(AsNumber(*lanes, &numeric_ok), 2.0, 1e-9);
    ORCMAP_EXPECT_TRUE(numeric_ok);
  }

  // --- place: one point feature -----------------------------------------
  const orcmap::MvtLayer* place = FindLayer(tile, "place");
  ORCMAP_EXPECT_TRUE(place != nullptr);
  if (place == nullptr) return;
  ORCMAP_EXPECT_EQ(place->features.size(), static_cast<size_t>(1));
  const orcmap::MvtFeature& town = place->features[0];
  ORCMAP_EXPECT_TRUE(town.geom_type == orcmap::MvtGeomType::kPoint);
  ORCMAP_EXPECT_EQ(town.geometry.size(), static_cast<size_t>(1));
  ORCMAP_EXPECT_EQ(town.geometry[0].size(), static_cast<size_t>(1));

  const orcmap::MvtValue* name = FindAttribute(town, "name");
  ORCMAP_EXPECT_TRUE(name != nullptr && std::holds_alternative<std::string>(*name));
  if (name != nullptr && std::holds_alternative<std::string>(*name)) {
    ORCMAP_EXPECT_TRUE(std::get<std::string>(*name) == "Testville");
  }
}

void TestEmptyBufferFails() {
  orcmap::MvtTile tile;
  const bool ok = orcmap::DecodeMvtTile(nullptr, 0, &tile);
  ORCMAP_EXPECT_TRUE(!ok);
}

void TestGarbageBufferFails() {
  const std::vector<uint8_t> garbage = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  orcmap::MvtTile tile;
  const bool ok = orcmap::DecodeMvtTile(garbage.data(), garbage.size(), &tile);
  ORCMAP_EXPECT_TRUE(!ok);
}

void TestTruncatedFixtureFails(const std::string& fixture_path) {
  std::vector<uint8_t> data = ReadFile(fixture_path);
  ORCMAP_EXPECT_TRUE(data.size() > 10);
  data.resize(data.size() / 2);  // Cut a valid tile in half.
  orcmap::MvtTile tile;
  const bool ok = orcmap::DecodeMvtTile(data.data(), data.size(), &tile);
  ORCMAP_EXPECT_TRUE(!ok);
}

}  // namespace

void RunMvtTests(const std::string& fixture_path) {
  TestDecodeFixture(fixture_path);
  TestEmptyBufferFails();
  TestGarbageBufferFails();
  TestTruncatedFixtureFails(fixture_path);
}
