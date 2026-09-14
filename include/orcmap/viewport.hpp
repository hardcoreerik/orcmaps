#pragma once

#include "orcmap/geo.hpp"

namespace orcmap {

// Minimal map viewport: center, zoom, and output size. All geographic /
// tile / screen transform math lives here, not in a graphics adapter.
//
// PARTIAL:
// - Overzoom (source tile z != viewport zoom) is rejected, not guessed.
// - Antimeridian wrap is not applied: (tile.x - center.x) is a raw
//   subtract. Shortest-world-distance wrap is a later Viewport slice
//   together with visible-tile enumeration. This struct does not bake in
//   a layout that would prevent that.
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

// Prepared per-source-tile map: Mercator center math runs once, then each
// vertex is origin + local * scale. Invalid when zoom is out of 0..31,
// source tile z != viewport zoom, or `out` is null.
struct TileScreenMap {
  bool valid = false;
  uint8_t zoom = 0;
  double origin_sx = 0.0;  // screen x of tile-local (0, 0)
  double origin_sy = 0.0;
  double scale = 0.0;      // screen pixels per tile-local unit
};

bool MakeTileScreenMap(const Viewport& viewport, TileId tile, uint32_t extent,
                       TileScreenMap* out);

// Cheap path after MakeTileScreenMap. Writes saturated int screen coords
// (non-finite maps to 0). `map.valid` must be true.
void ProjectLocal(const TileScreenMap& map, int32_t local_x, int32_t local_y,
                  int* screen_x, int* screen_y);

// Convenience wrapper: prepare + project. No-op if pointers are null.
// Invalid zoom / zoom mismatch leaves outputs unchanged.
void TileLocalToScreen(const Viewport& viewport, TileId tile, uint32_t extent,
                       int32_t local_x, int32_t local_y, int* screen_x,
                       int* screen_y);

}  // namespace orcmap
