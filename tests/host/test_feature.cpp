#include "orcmap/experimental/mvt_classify.hpp"
#include "orcmap/feature.hpp"
#include "orcmap/mvt.hpp"
#include "orcmap/mvt_translate.hpp"

#include <algorithm>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "test_util.hpp"

namespace {

std::vector<uint8_t> ReadFile(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),
                               std::istreambuf_iterator<char>());
}

const orcmap::Feature* FindByLayer(const orcmap::FeatureTile& tile,
                                   const std::string& layer) {
  for (const auto& feature : tile.features) {
    if (feature.layer == layer) return &feature;
  }
  return nullptr;
}

void TestEmptyTileTranslatesEmpty() {
  orcmap::MvtTile empty;
  orcmap::FeatureTile out;
  out.features.push_back(orcmap::Feature{});
  ORCMAP_EXPECT_TRUE(orcmap::TranslateMvtToFeatureTile(empty, &out));
  ORCMAP_EXPECT_EQ(out.features.size(), static_cast<size_t>(0));
}

void TestNullOutFails() {
  orcmap::MvtTile tile;
  ORCMAP_EXPECT_TRUE(!orcmap::TranslateMvtToFeatureTile(tile, nullptr));
}

void TestUnknownGeomCopied() {
  orcmap::MvtTile mvt;
  orcmap::MvtLayer layer;
  layer.name = "mystery";
  layer.extent = 256;
  orcmap::MvtFeature feat;
  feat.id = 42;
  feat.geom_type = orcmap::MvtGeomType::kUnknown;
  feat.geometry.push_back({orcmap::MvtPoint{1, 2}, orcmap::MvtPoint{3, 4}});
  feat.attribute_keys.push_back("note");
  feat.attribute_values.emplace_back(std::string("x"));
  layer.features.push_back(feat);
  mvt.layers.push_back(layer);

  orcmap::FeatureTile out;
  ORCMAP_EXPECT_TRUE(orcmap::TranslateMvtToFeatureTile(mvt, &out));
  ORCMAP_EXPECT_EQ(out.features.size(), static_cast<size_t>(1));
  const orcmap::Feature& f = out.features[0];
  ORCMAP_EXPECT_EQ(f.id, static_cast<uint64_t>(42));
  ORCMAP_EXPECT_TRUE(f.geom_type == orcmap::GeomType::kUnknown);
  ORCMAP_EXPECT_TRUE(f.geometry.type == orcmap::GeomType::kUnknown);
  ORCMAP_EXPECT_TRUE(!f.kind_assigned);
  ORCMAP_EXPECT_EQ(f.extent, 256u);
  ORCMAP_EXPECT_TRUE(f.layer == "mystery");
  ORCMAP_EXPECT_EQ(f.geometry.paths.size(), static_cast<size_t>(1));
  ORCMAP_EXPECT_EQ(f.geometry.paths[0].size(), static_cast<size_t>(2));
  ORCMAP_EXPECT_EQ(f.geometry.paths[0][0].x, 1);
  ORCMAP_EXPECT_EQ(f.geometry.paths[0][1].y, 4);
}

void TestFeatureTileOwnsCopyAfterMvtDestroyed() {
  orcmap::FeatureTile out;
  {
    orcmap::MvtTile mvt;
    orcmap::MvtLayer layer;
    layer.name = "water";
    layer.extent = 4096;
    orcmap::MvtFeature feat;
    feat.geom_type = orcmap::MvtGeomType::kPoint;
    feat.geometry.push_back({orcmap::MvtPoint{9, 8}});
    feat.attribute_keys.push_back("name");
    feat.attribute_values.emplace_back(std::string("owned-copy"));
    layer.features.push_back(std::move(feat));
    mvt.layers.push_back(std::move(layer));
    ORCMAP_EXPECT_TRUE(orcmap::TranslateMvtToFeatureTile(mvt, &out));
  }
  ORCMAP_EXPECT_EQ(out.features.size(), static_cast<size_t>(1));
  ORCMAP_EXPECT_TRUE(out.features[0].layer == "water");
  ORCMAP_EXPECT_EQ(out.features[0].geometry.paths[0][0].x, 9);
  const orcmap::PropertyValue* name =
      orcmap::FindProperty(out.features[0], "name");
  ORCMAP_EXPECT_TRUE(name != nullptr &&
                     std::holds_alternative<std::string>(*name));
  if (name != nullptr && std::holds_alternative<std::string>(*name)) {
    ORCMAP_EXPECT_TRUE(std::get<std::string>(*name) == "owned-copy");
  }
}

void TestTranslateFixture(const std::string& fixture_path) {
  const std::vector<uint8_t> data = ReadFile(fixture_path);
  ORCMAP_EXPECT_TRUE(!data.empty());
  orcmap::MvtTile mvt;
  ORCMAP_EXPECT_TRUE(orcmap::DecodeMvtTile(data.data(), data.size(), &mvt));

  orcmap::FeatureTile tile;
  ORCMAP_EXPECT_TRUE(orcmap::TranslateMvtToFeatureTile(mvt, &tile));
  ORCMAP_EXPECT_EQ(tile.features.size(), static_cast<size_t>(3));

  const orcmap::Feature* water = FindByLayer(tile, "water");
  ORCMAP_EXPECT_TRUE(water != nullptr);
  if (water != nullptr) {
    ORCMAP_EXPECT_TRUE(water->geom_type == orcmap::GeomType::kPolygon);
    ORCMAP_EXPECT_TRUE(!water->kind_assigned);
    ORCMAP_EXPECT_EQ(water->extent, 4096u);
    ORCMAP_EXPECT_EQ(water->geometry.paths.size(), static_cast<size_t>(1));
    ORCMAP_EXPECT_EQ(water->geometry.paths[0].size(), static_cast<size_t>(4));
    int32_t min_x = water->geometry.paths[0][0].x;
    int32_t max_x = min_x;
    int32_t min_y = water->geometry.paths[0][0].y;
    int32_t max_y = min_y;
    for (const auto& p : water->geometry.paths[0]) {
      min_x = std::min(min_x, p.x);
      max_x = std::max(max_x, p.x);
      min_y = std::min(min_y, p.y);
      max_y = std::max(max_y, p.y);
    }
    ORCMAP_EXPECT_EQ(max_x - min_x, 4);
    ORCMAP_EXPECT_EQ(max_y - min_y, 4);
    const orcmap::PropertyValue* kind = orcmap::FindProperty(*water, "kind");
    ORCMAP_EXPECT_TRUE(kind != nullptr &&
                       std::holds_alternative<std::string>(*kind));
  }

  const orcmap::Feature* road = FindByLayer(tile, "road");
  ORCMAP_EXPECT_TRUE(road != nullptr);
  if (road != nullptr) {
    ORCMAP_EXPECT_TRUE(road->geom_type == orcmap::GeomType::kLineString);
    ORCMAP_EXPECT_TRUE(!road->kind_assigned);
    ORCMAP_EXPECT_EQ(road->geometry.paths.size(), static_cast<size_t>(1));
    ORCMAP_EXPECT_EQ(road->geometry.paths[0].size(), static_cast<size_t>(3));
    const orcmap::PropertyValue* oneway = orcmap::FindProperty(*road, "oneway");
    ORCMAP_EXPECT_TRUE(oneway != nullptr && std::holds_alternative<bool>(*oneway));
    if (oneway != nullptr && std::holds_alternative<bool>(*oneway)) {
      ORCMAP_EXPECT_TRUE(std::get<bool>(*oneway) == true);
    }
  }

  const orcmap::Feature* place = FindByLayer(tile, "place");
  ORCMAP_EXPECT_TRUE(place != nullptr);
  if (place != nullptr) {
    ORCMAP_EXPECT_TRUE(place->geom_type == orcmap::GeomType::kPoint);
    ORCMAP_EXPECT_TRUE(!place->kind_assigned);
    ORCMAP_EXPECT_EQ(place->geometry.paths.size(), static_cast<size_t>(1));
    ORCMAP_EXPECT_EQ(place->geometry.paths[0].size(), static_cast<size_t>(1));
    const orcmap::PropertyValue* name = orcmap::FindProperty(*place, "name");
    ORCMAP_EXPECT_TRUE(name != nullptr &&
                       std::holds_alternative<std::string>(*name));
    if (name != nullptr && std::holds_alternative<std::string>(*name)) {
      ORCMAP_EXPECT_TRUE(std::get<std::string>(*name) == "Testville");
    }
  }
}

void TestExperimentalClassifyFixture(const std::string& fixture_path) {
  const std::vector<uint8_t> data = ReadFile(fixture_path);
  orcmap::MvtTile mvt;
  ORCMAP_EXPECT_TRUE(orcmap::DecodeMvtTile(data.data(), data.size(), &mvt));
  orcmap::FeatureTile tile;
  ORCMAP_EXPECT_TRUE(orcmap::TranslateMvtToFeatureTile(mvt, &tile));
  ORCMAP_EXPECT_TRUE(orcmap::experimental::AssignFeatureKinds(&tile));

  const orcmap::Feature* water = FindByLayer(tile, "water");
  const orcmap::Feature* road = FindByLayer(tile, "road");
  const orcmap::Feature* place = FindByLayer(tile, "place");
  ORCMAP_EXPECT_TRUE(water != nullptr && water->kind_assigned &&
                     water->kind == orcmap::FeatureKind::kWater);
  ORCMAP_EXPECT_TRUE(road != nullptr && road->kind_assigned &&
                     road->kind == orcmap::FeatureKind::kPrimaryRoad);
  ORCMAP_EXPECT_TRUE(place != nullptr && place->kind_assigned &&
                     place->kind == orcmap::FeatureKind::kLabelPrimary);
}

void TestExperimentalLeavesUnknownUnassigned() {
  orcmap::FeatureTile tile;
  orcmap::Feature unknown;
  unknown.layer = "not-a-real-schema-layer";
  unknown.geom_type = orcmap::GeomType::kPoint;
  tile.features.push_back(unknown);
  ORCMAP_EXPECT_TRUE(orcmap::experimental::AssignFeatureKinds(&tile));
  ORCMAP_EXPECT_TRUE(!tile.features[0].kind_assigned);
}

void TestAssignKindsNullFails() {
  ORCMAP_EXPECT_TRUE(!orcmap::experimental::AssignFeatureKinds(nullptr));
}

}  // namespace

void RunFeatureTests(const std::string& mvt_fixture_path) {
  TestEmptyTileTranslatesEmpty();
  TestNullOutFails();
  TestUnknownGeomCopied();
  TestFeatureTileOwnsCopyAfterMvtDestroyed();
  TestTranslateFixture(mvt_fixture_path);
  TestExperimentalClassifyFixture(mvt_fixture_path);
  TestExperimentalLeavesUnknownUnassigned();
  TestAssignKindsNullFails();
}
