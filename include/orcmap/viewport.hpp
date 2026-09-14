#pragma once

#include "orcmap/geo.hpp"

namespace orcmap {

// Minimal map viewport: center, zoom, and output size. All geographic /
// tile / screen transform math for the first renderer lives here, not in
// a graphics adapter. PARTIAL: overzoom (source tile z != viewport zoom)
// is not handled -- RenderFeatureTile requires matching zoom.
//
// tile_size_px is screen pixels per map tile at `zoom` (Web Mercator
// convention defaults to 256). Tests may set it equal to the framebuffer
// so one z0 tile fills the target.

struct Viewport {
  double center_lat_deg = 0.0;
  double center_lon_deg = 0.0;
  uint8_t zoom = 0;
  int width_px = 256;
  int height_px = 256;
  int tile_size_px = 256;
};

// Maps a tile-local point on `tile` into screen pixels (origin top-left).
// Result may lie outside [0, width) x [0, height); callers clip.
// `extent` 0 is treated as 1. `tile.z` should match `viewport.zoom`.
void TileLocalToScreen(const Viewport& viewport, TileId tile, uint32_t extent,
                       int32_t local_x, int32_t local_y, int* screen_x,
                       int* screen_y);

}  // namespace orcmap
