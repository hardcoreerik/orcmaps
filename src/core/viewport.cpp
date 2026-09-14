#include "orcmap/viewport.hpp"

#include <cmath>
#include <limits>

namespace orcmap {

namespace {

int SaturateToInt(double v) {
  if (!std::isfinite(v)) return 0;
  const double hi = static_cast<double>(std::numeric_limits<int>::max());
  const double lo = static_cast<double>(std::numeric_limits<int>::min());
  if (v >= hi) return std::numeric_limits<int>::max();
  if (v <= lo) return std::numeric_limits<int>::min();
  return static_cast<int>(std::floor(v));
}

}  // namespace

bool MakeTileScreenMap(const Viewport& viewport, TileId tile, uint32_t extent,
                       TileScreenMap* out) {
  if (out == nullptr) return false;
  out->valid = false;
  if (!ZoomIsValid(viewport.zoom)) return false;
  if (tile.z != viewport.zoom) return false;
  const double ext = extent == 0 ? 1.0 : static_cast<double>(extent);
  const double tile_px = viewport.tile_size_px <= 0
                             ? 1.0
                             : static_cast<double>(viewport.tile_size_px);
  const TileCoord center = LatLonToTileCoord(
      viewport.center_lat_deg, viewport.center_lon_deg, viewport.zoom);
  // Raw subtract: antimeridian wrap is PARTIAL (see viewport.hpp).
  const double origin_sx =
      (static_cast<double>(tile.x) - center.x) * tile_px +
      static_cast<double>(viewport.width_px) * 0.5;
  const double origin_sy =
      (static_cast<double>(tile.y) - center.y) * tile_px +
      static_cast<double>(viewport.height_px) * 0.5;
  out->zoom = viewport.zoom;
  out->origin_sx = origin_sx;
  out->origin_sy = origin_sy;
  out->scale = tile_px / ext;
  out->valid = true;
  return true;
}

void ProjectLocal(const TileScreenMap& map, int32_t local_x, int32_t local_y,
                  int* screen_x, int* screen_y) {
  if (!map.valid) return;
  const double sx = map.origin_sx + static_cast<double>(local_x) * map.scale;
  const double sy = map.origin_sy + static_cast<double>(local_y) * map.scale;
  if (screen_x != nullptr) *screen_x = SaturateToInt(sx);
  if (screen_y != nullptr) *screen_y = SaturateToInt(sy);
}

void TileLocalToScreen(const Viewport& viewport, TileId tile, uint32_t extent,
                       int32_t local_x, int32_t local_y, int* screen_x,
                       int* screen_y) {
  if (screen_x == nullptr || screen_y == nullptr) return;
  TileScreenMap map;
  if (!MakeTileScreenMap(viewport, tile, extent, &map)) return;
  ProjectLocal(map, local_x, local_y, screen_x, screen_y);
}

}  // namespace orcmap
