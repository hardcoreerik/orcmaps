#include "orcmap/viewport.hpp"

#include <cmath>

namespace orcmap {

void TileLocalToScreen(const Viewport& viewport, TileId tile, uint32_t extent,
                       int32_t local_x, int32_t local_y, int* screen_x,
                       int* screen_y) {
  if (screen_x == nullptr || screen_y == nullptr) return;
  const double ext = extent == 0 ? 1.0 : static_cast<double>(extent);
  const TileCoord center = LatLonToTileCoord(
      viewport.center_lat_deg, viewport.center_lon_deg, viewport.zoom);
  const double tile_px = viewport.tile_size_px <= 0
                             ? 1.0
                             : static_cast<double>(viewport.tile_size_px);
  const double fx =
      static_cast<double>(tile.x) + static_cast<double>(local_x) / ext;
  const double fy =
      static_cast<double>(tile.y) + static_cast<double>(local_y) / ext;
  const double sx =
      (fx - center.x) * tile_px + static_cast<double>(viewport.width_px) * 0.5;
  const double sy =
      (fy - center.y) * tile_px + static_cast<double>(viewport.height_px) * 0.5;
  *screen_x = static_cast<int>(std::floor(sx));
  *screen_y = static_cast<int>(std::floor(sy));
}

}  // namespace orcmap
