// Deterministic 1280x720 host preview of the current OrcMaps renderer.
// Synthetic FeatureTiles only -- not a schema decision and not real map data.
// Regenerates a P6 PPM; do not treat a committed image as project truth.

#include "framebuffer_target.hpp"
#include "orcmap/feature.hpp"
#include "orcmap/renderer.hpp"
#include "orcmap/style.hpp"
#include "orcmap/viewport.hpp"

#include <cstdio>
#include <string>
#include <utility>

namespace {

orcmap::Feature Poly(orcmap::FeatureKind kind, orcmap::Path ring) {
  orcmap::Feature f;
  f.kind = kind;
  f.kind_assigned = true;
  f.extent = 4096;
  f.geometry.type = orcmap::GeomType::kPolygon;
  f.geometry.paths.push_back(std::move(ring));
  return f;
}

orcmap::Feature Line(orcmap::FeatureKind kind, orcmap::Path path) {
  orcmap::Feature f;
  f.kind = kind;
  f.kind_assigned = true;
  f.extent = 4096;
  f.geometry.type = orcmap::GeomType::kLineString;
  f.geometry.paths.push_back(std::move(path));
  return f;
}

orcmap::FeatureTile MakeScene() {
  orcmap::FeatureTile tile;
  tile.features.push_back(Poly(orcmap::FeatureKind::kLand,
                               {{0, 0}, {4096, 0}, {4096, 4096}, {0, 4096}}));
  tile.features.push_back(Poly(
      orcmap::FeatureKind::kWater,
      {{180, 1700}, {2400, 1500}, {3100, 2700}, {500, 3100}, {180, 1700}}));
  tile.features.push_back(
      Line(orcmap::FeatureKind::kMotorway, {{0, 2100}, {4096, 1850}}));
  tile.features.push_back(
      Line(orcmap::FeatureKind::kPrimaryRoad, {{800, 0}, {1200, 4096}}));
  tile.features.push_back(
      Line(orcmap::FeatureKind::kSecondaryRoad, {{0, 3200}, {4096, 3400}}));
  orcmap::Feature town;
  town.kind = orcmap::FeatureKind::kLabelPrimary;
  town.kind_assigned = true;
  town.extent = 4096;
  town.geometry.type = orcmap::GeomType::kPoint;
  town.geometry.paths.push_back({{2100, 2300}});
  tile.features.push_back(town);
  return tile;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string path = argc > 1 ? argv[1] : "orcmap-preview.ppm";
  const int width = 1280;
  const int height = 720;
  orcmap::host::FramebufferTarget fb(width, height);
  orcmap::Viewport viewport;
  viewport.center_lat_deg = 0.0;
  viewport.center_lon_deg = 0.0;
  viewport.zoom = 0;
  viewport.width_px = width;
  viewport.height_px = height;
  viewport.tile_size_px = height;
  const orcmap::MapStyle& style = orcmap::styles::OrcSdrDark();
  const orcmap::FeatureTile scene = MakeScene();
  if (!orcmap::ClearMapBackground(viewport, style, &fb)) return 1;
  if (!orcmap::RenderFeatureTile(scene, orcmap::TileId{0, 0, 0}, viewport,
                                 style, &fb)) {
    return 1;
  }
  if (!fb.WritePpm(path)) return 1;
  std::printf("wrote %s (%dx%d)\n", path.c_str(), width, height);
  return 0;
}
