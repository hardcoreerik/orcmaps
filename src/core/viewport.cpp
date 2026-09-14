#include "orcmap/viewport.hpp"

#include <cmath>
#include <cstdint>
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

// Shortest signed X delta in tile units, range [-n/2, n/2).
double WrappedTileDeltaX(double tile_x, double center_x, uint32_t n) {
  if (n == 0) return 0.0;
  const double nd = static_cast<double>(n);
  double dx = tile_x - center_x;
  dx -= nd * std::floor((dx + nd * 0.5) / nd);
  return dx;
}

uint32_t WrapTileX(int64_t x, uint32_t n) {
  const int64_t n64 = static_cast<int64_t>(n);
  int64_t w = x % n64;
  if (w < 0) w += n64;
  return static_cast<uint32_t>(w);
}

}  // namespace

bool MakeTileScreenMap(const Viewport& viewport, TileId tile, uint32_t extent,
                       TileScreenMap* out) {
  if (out == nullptr) return false;
  out->valid = false;
  if (!ZoomIsValid(viewport.zoom)) return false;
  if (tile.z != viewport.zoom) return false;
  const uint32_t n = TilesPerAxis(viewport.zoom);
  if (n == 0 || tile.x >= n || tile.y >= n) return false;
  const double ext = extent == 0 ? 1.0 : static_cast<double>(extent);
  const double tile_px = viewport.tile_size_px <= 0
                             ? 1.0
                             : static_cast<double>(viewport.tile_size_px);
  const TileCoord center = LatLonToTileCoord(
      viewport.center_lat_deg, viewport.center_lon_deg, viewport.zoom);
  const double dx = WrappedTileDeltaX(static_cast<double>(tile.x), center.x, n);
  const double origin_sx =
      dx * tile_px + static_cast<double>(viewport.width_px) * 0.5;
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

bool EnumerateVisibleTiles(const Viewport& viewport, std::vector<TileId>* out) {
  if (out == nullptr) return false;
  out->clear();
  if (!ZoomIsValid(viewport.zoom)) return false;
  if (viewport.width_px <= 0 || viewport.height_px <= 0) return false;
  if (viewport.tile_size_px <= 0) return false;
  const uint32_t n = TilesPerAxis(viewport.zoom);
  if (n == 0) return false;

  const TileCoord center = LatLonToTileCoord(
      viewport.center_lat_deg, viewport.center_lon_deg, viewport.zoom);
  const double ts = static_cast<double>(viewport.tile_size_px);
  const double half_w = static_cast<double>(viewport.width_px) * 0.5 / ts;
  const double half_h = static_cast<double>(viewport.height_px) * 0.5 / ts;
  const double left = center.x - half_w;
  const double right = center.x + half_w;
  const double top = center.y - half_h;
  const double bottom = center.y + half_h;

  // Tiles whose [i, i+1) overlaps [left, right) / [top, bottom).
  int64_t x0 = static_cast<int64_t>(std::floor(left));
  int64_t x1 = static_cast<int64_t>(std::ceil(right)) - 1;
  int64_t y0 = static_cast<int64_t>(std::floor(top));
  int64_t y1 = static_cast<int64_t>(std::ceil(bottom)) - 1;
  if (x1 < x0) return true;

  const int64_t n64 = static_cast<int64_t>(n);
  if (y1 < 0 || y0 >= n64) return true;
  if (y0 < 0) y0 = 0;
  if (y1 >= n64) y1 = n64 - 1;
  if (y0 > y1) return true;

  int64_t x_count = x1 - x0 + 1;
  if (x_count > n64) x_count = n64;

  out->reserve(static_cast<size_t>(x_count) * static_cast<size_t>(y1 - y0 + 1));
  for (int64_t y = y0; y <= y1; ++y) {
    for (int64_t k = 0; k < x_count; ++k) {
      out->push_back(TileId{viewport.zoom, WrapTileX(x0 + k, n),
                            static_cast<uint32_t>(y)});
    }
  }
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
