#include "orcmap/map_source.hpp"

#include "test_util.hpp"

namespace {

orcmap::MapSourceInfo MakeSource(std::string id, bool requires_attribution,
                                  std::string text) {
  orcmap::MapSourceInfo source;
  source.id = std::move(id);
  source.attribution.required = requires_attribution;
  source.attribution.text = std::move(text);
  return source;
}

void TestNoSourcesRequireNothing() {
  std::vector<orcmap::MapSourceInfo> sources;
  ORCMAP_EXPECT_TRUE(orcmap::CollectRequiredAttribution(sources).empty());
}

void TestSourceWithoutAttributionIsExcluded() {
  std::vector<orcmap::MapSourceInfo> sources = {
      MakeSource("natural-earth", false, ""),
  };
  ORCMAP_EXPECT_TRUE(orcmap::CollectRequiredAttribution(sources).empty());
}

void TestSourceRequiringAttributionIsIncluded() {
  std::vector<orcmap::MapSourceInfo> sources = {
      MakeSource("openstreetmap", true, "(c) OpenStreetMap contributors"),
  };
  const auto required = orcmap::CollectRequiredAttribution(sources);
  ORCMAP_EXPECT_EQ(required.size(), static_cast<size_t>(1));
  ORCMAP_EXPECT_TRUE(required[0].text == "(c) OpenStreetMap contributors");
}

void TestMixedSourcesOnlyCollectRequired() {
  std::vector<orcmap::MapSourceInfo> sources = {
      MakeSource("natural-earth", false, ""),
      MakeSource("openstreetmap", true, "(c) OpenStreetMap contributors"),
      MakeSource("geoboundaries-gbopen", true, "geoBoundaries"),
  };
  const auto required = orcmap::CollectRequiredAttribution(sources);
  ORCMAP_EXPECT_EQ(required.size(), static_cast<size_t>(2));
}

void TestDuplicateAttributionTextIsDeduplicated() {
  // Two OSM-derived sources (e.g. an osm-detail pack and a Google Open
  // Buildings pack whose ODbL option was chosen) can require the exact
  // same credit line -- it should only be surfaced once.
  std::vector<orcmap::MapSourceInfo> sources = {
      MakeSource("openstreetmap", true, "(c) OpenStreetMap contributors"),
      MakeSource("osm-detail-layer-2", true, "(c) OpenStreetMap contributors"),
  };
  const auto required = orcmap::CollectRequiredAttribution(sources);
  ORCMAP_EXPECT_EQ(required.size(), static_cast<size_t>(1));
}

}  // namespace

void RunAttributionTests() {
  TestNoSourcesRequireNothing();
  TestSourceWithoutAttributionIsExcluded();
  TestSourceRequiringAttributionIsIncluded();
  TestMixedSourcesOnlyCollectRequired();
  TestDuplicateAttributionTextIsDeduplicated();
}
