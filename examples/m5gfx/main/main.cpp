// Compile/API proof: a generic ESP-IDF app can render through OrcMaps'
// RenderTarget implemented by M5GFX/LovyanGFX. No M5Unified, no OrcSDR,
// no MVT types in the adapter. Uses an off-screen M5Canvas so this does
// not require a particular board at compile time.
//
// Same engine calls as the host framebuffer:
//   ClearMapBackground(...)
//   RenderFeatureTile(...)

#include <M5GFX.h>
#include "orcmap/m5gfx/display_target.hpp"
#include "esp_log.h"
#include "orcmap/feature.hpp"
#include "orcmap/renderer.hpp"
#include "orcmap/style.hpp"
#include "orcmap/viewport.hpp"

static const char* kTag = "orcmap_m5gfx";

static orcmap::FeatureTile MakeSyntheticTile() {
  orcmap::FeatureTile tile;
  orcmap::Feature land;
  land.kind = orcmap::FeatureKind::kLand;
  land.kind_assigned = true;
  land.extent = 4096;
  land.geometry.type = orcmap::GeomType::kPolygon;
  land.geometry.paths.push_back({{0, 0}, {4096, 0}, {4096, 4096}, {0, 4096}});
  tile.features.push_back(land);

  orcmap::Feature water;
  water.kind = orcmap::FeatureKind::kWater;
  water.kind_assigned = true;
  water.extent = 4096;
  water.geometry.type = orcmap::GeomType::kPolygon;
  water.geometry.paths.push_back(
      {{200, 1800}, {2200, 1600}, {2800, 2800}, {400, 3000}});
  tile.features.push_back(water);

  orcmap::Feature road;
  road.kind = orcmap::FeatureKind::kMotorway;
  road.kind_assigned = true;
  road.extent = 4096;
  road.geometry.type = orcmap::GeomType::kLineString;
  road.geometry.paths.push_back({{0, 2200}, {4096, 1900}});
  tile.features.push_back(road);
  return tile;
}

extern "C" void app_main(void) {
  M5Canvas canvas;
  canvas.createSprite(160, 120);

  orcmap::m5gfx_adapter::DisplayTarget target(canvas);

  orcmap::Viewport viewport;
  viewport.center_lat_deg = 0.0;
  viewport.center_lon_deg = 0.0;
  viewport.zoom = 0;
  viewport.width_px = target.Width();
  viewport.height_px = target.Height();
  viewport.tile_size_px =
      target.Width() < target.Height() ? target.Width() : target.Height();

  const orcmap::MapStyle& style = orcmap::styles::OrcSdrDark();
  const orcmap::FeatureTile features = MakeSyntheticTile();

  const bool cleared = orcmap::ClearMapBackground(viewport, style, &target);
  const bool drawn = orcmap::RenderFeatureTile(
      features, orcmap::TileId{0, 0, 0}, viewport, style, &target);

  ESP_LOGI(kTag, "canvas %dx%d clear=%d draw=%d", target.Width(),
           target.Height(), static_cast<int>(cleared),
           static_cast<int>(drawn));
}
