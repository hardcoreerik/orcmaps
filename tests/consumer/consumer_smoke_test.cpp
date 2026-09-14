// External-consumer smoke test.
//
// This file may ONLY #include headers reachable through OrcMaps' public
// surface: include/orcmap/** (the public API) and adapters/host/** (a
// public integration adapter -- the same kind of thing OrcSDR would use
// adapters/esp_idf for). It must never reach into src/** or third_party/**
// directly; CMakeLists.txt in this directory enforces that structurally by
// simply never adding those directories to this target's include path, so
// a violation here is a build failure, not a code-review-only rule.
//
// This exercises what's real today (docs/STATUS.md): tile-coordinate math,
// opening a real PMTiles archive and reading a tile by z/x/y, decoding a
// real MVT vector tile, translating it into the OrcMaps Feature model,
// resolving a built-in style, and the runtime attribution API. It
// deliberately does NOT exercise MapEngine/Viewport/rendering/overlays,
// because none of those exist yet -- see docs/ARCHITECTURE.md.

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "file_byte_source.hpp"       // adapters/host -- public integration adapter.
#include "orcmap/attribution.hpp"     // include/orcmap -- public API.
#include "orcmap/geo.hpp"
#include "orcmap/map_source.hpp"
#include "orcmap/feature.hpp"
#include "orcmap/mvt.hpp"
#include "orcmap/mvt_translate.hpp"
#include "orcmap/pmtiles.hpp"
#include "orcmap/style.hpp"

namespace {

bool CheckTileMath() {
  const orcmap::TileId tile = orcmap::LatLonToTile(44.0521, -123.0868, 10);
  const orcmap::LatLon corner = orcmap::TileToLatLon(tile);
  return tile.z == 10 && corner.lat_deg > 43.0 && corner.lat_deg < 45.0;
}

bool CheckPmTilesOpenAndRead(const std::string& fixture_path) {
  orcmap::host::FileByteSource source(fixture_path);
  if (!source.Valid()) return false;
  orcmap::PmTilesReader reader(&source);
  if (!reader.Open()) return false;
  std::vector<uint8_t> tile_bytes;
  return reader.GetTile(0, 0, 0, &tile_bytes) && !tile_bytes.empty();
}

bool CheckStyleAndAttribution() {
  const orcmap::MapStyle& style = orcmap::styles::OrcSdrDark();
  const orcmap::MapPaint paint =
      orcmap::ResolveFeatureStyle(orcmap::FeatureKind::kMotorway, 5, style);
  if (!paint.visible) return false;

  orcmap::MapSourceInfo osm_source;
  osm_source.id = "openstreetmap";
  osm_source.attribution.required = true;
  osm_source.attribution.text = "(c) OpenStreetMap contributors";

  orcmap::MapSourceInfo clean_source;
  clean_source.id = "natural-earth";
  clean_source.attribution.required = false;

  const auto required = orcmap::CollectRequiredAttribution({osm_source, clean_source});
  return required.size() == 1 && required[0].text == "(c) OpenStreetMap contributors";
}

bool CheckMvtDecode(const std::string& mvt_fixture_path) {
  std::ifstream f(mvt_fixture_path, std::ios::binary);
  const std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)),
                                   std::istreambuf_iterator<char>());
  if (data.empty()) return false;
  orcmap::MvtTile tile;
  if (!orcmap::DecodeMvtTile(data.data(), data.size(), &tile)) return false;
  return tile.layers.size() == 3;
}

bool CheckMvtTranslate(const std::string& mvt_fixture_path) {
  std::ifstream f(mvt_fixture_path, std::ios::binary);
  const std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)),
                                   std::istreambuf_iterator<char>());
  if (data.empty()) return false;
  orcmap::MvtTile mvt;
  if (!orcmap::DecodeMvtTile(data.data(), data.size(), &mvt)) return false;
  orcmap::FeatureTile features;
  if (!orcmap::TranslateMvtToFeatureTile(mvt, &features)) return false;
  if (features.features.size() != 3) return false;
  return !features.features[0].kind_assigned;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string fixture_path =
      argc > 1 ? argv[1] : "tests/fixtures/tiny.pmtiles";
  const std::string mvt_fixture_path =
      argc > 2 ? argv[2] : "tests/fixtures/tiny.mvt";

  bool ok = true;
  ok &= CheckTileMath();
  ok &= CheckPmTilesOpenAndRead(fixture_path);
  ok &= CheckStyleAndAttribution();
  ok &= CheckMvtDecode(mvt_fixture_path);
  ok &= CheckMvtTranslate(mvt_fixture_path);

  if (ok) {
    std::printf("PASS: OrcMaps consumer smoke test (public headers only)\n");
    return 0;
  }
  std::printf("FAIL: OrcMaps consumer smoke test\n");
  return 1;
}
