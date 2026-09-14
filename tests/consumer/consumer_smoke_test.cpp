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
// resolving a built-in style, host framebuffer render of an empty tile,
// and the runtime attribution API.

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "file_byte_source.hpp"       // adapters/host -- public integration adapter.
#include "framebuffer_target.hpp"
#include "orcmap/attribution.hpp"     // include/orcmap -- public API.
#include "orcmap/feature.hpp"
#include "orcmap/feature_kind.hpp"
#include "orcmap/compression.hpp"
#include "orcmap/geo.hpp"
#include "orcmap/map_source.hpp"
#include "orcmap/mvt.hpp"
#include "orcmap/mvt_translate.hpp"
#include "orcmap/pack.hpp"
#include "orcmap/pmtiles.hpp"
#include "orcmap/renderer.hpp"
#include "orcmap/style.hpp"
#include "orcmap/viewport.hpp"

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

bool CheckGzipTilePipeline(const std::string& gzip_pmtiles) {
  orcmap::host::FileByteSource source(gzip_pmtiles);
  if (!source.Valid()) return false;
  orcmap::PmTilesReader reader(&source);
  if (!reader.Open()) return false;
  if (reader.Header().tile_compression != orcmap::Compression::kGzip) return false;
  std::vector<uint8_t> stored;
  if (!reader.GetTile(0, 0, 0, &stored) || stored.empty()) return false;
  std::vector<uint8_t> raw;
  if (!orcmap::DecompressPayload(orcmap::Compression::kGzip, stored.data(),
                                 stored.size(), 65536, &raw)) {
    return false;
  }
  orcmap::MvtTile mvt;
  return orcmap::DecodeMvtTile(raw.data(), raw.size(), &mvt) &&
         mvt.layers.size() == 3;
}

bool CheckHostRender() {
  orcmap::host::FramebufferTarget fb(8, 8);
  orcmap::FeatureTile empty;
  orcmap::Viewport v;
  v.center_lat_deg = 0;
  v.center_lon_deg = 0;
  v.zoom = 0;
  v.width_px = 8;
  v.height_px = 8;
  v.tile_size_px = 8;
  if (!orcmap::ClearMapBackground(v, orcmap::styles::OrcSdrDark(), &fb)) {
    return false;
  }
  if (!orcmap::RenderFeatureTile(empty, orcmap::TileId{0, 0, 0}, v,
                                 orcmap::styles::OrcSdrDark(), &fb)) {
    return false;
  }
  return fb.At(0, 0) == orcmap::Color::Rgb(8, 10, 12);
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

bool CheckPackResolution() {
  orcmap::PackManifest pack;
  pack.pack_version = "2026.09";
  pack.display_name = "World";
  pack.region_id = "world";
  pack.region_name = "World";
  pack.bounds = {-180.0, -85.0, 180.0, 85.0};
  pack.min_zoom = 0;
  pack.max_zoom = 7;
  pack.content_profile = "overview";
  pack.pmtiles_version = 3;
  pack.schema_version = "openmaptiles-3.16";
  pack.source_snapshot = "2026-09";
  pack.builder = "consumer-test";
  pack.builder_version = "1";
  pack.builder_commit = std::string(40, 'a');
  pack.build_date = "2026-09-14";
  pack.provenance_ids = {"test-source"};
  pack.pack_class = "clean";
  pack.size_bytes = 1;
  pack.output_sha256 = std::string(64, 'b');
  pack.archive_path = "/orcmaps/packs/world/map.pmtiles";
  pack.pack_id = orcmap::MakePackId(pack);
  orcmap::PackCatalog catalog;
  if (!catalog.Add(pack)) return false;
  const auto* selected =
      orcmap::ResolvePack(catalog, {-1.0, -1.0, 1.0, 1.0}, 4);
  return selected != nullptr && selected->pack_id == pack.pack_id;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string fixture_path =
      argc > 1 ? argv[1] : "tests/fixtures/tiny.pmtiles";
  const std::string mvt_fixture_path =
      argc > 2 ? argv[2] : "tests/fixtures/tiny.mvt";
  const std::string gzip_pmtiles =
      argc > 3 ? argv[3] : "tests/fixtures/tiny-gzip.pmtiles";

  bool ok = true;
  ok &= CheckTileMath();
  ok &= CheckPmTilesOpenAndRead(fixture_path);
  ok &= CheckStyleAndAttribution();
  ok &= CheckMvtDecode(mvt_fixture_path);
  ok &= CheckMvtTranslate(mvt_fixture_path);
  ok &= CheckGzipTilePipeline(gzip_pmtiles);
  ok &= CheckHostRender();
  ok &= CheckPackResolution();

  if (ok) {
    std::printf("PASS: OrcMaps consumer smoke test (public headers only)\n");
    return 0;
  }
  std::printf("FAIL: OrcMaps consumer smoke test\n");
  return 1;
}
