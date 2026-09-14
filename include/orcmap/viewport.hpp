#pragma once

#include <vector>

#include "orcmap/geo.hpp"

namespace orcmap {

// Map viewport: center, zoom, and output size. Geographic / tile / screen
// transform math lives here, not in a graphics adapter.
//
// Overzoom (source tile z != viewport zoom) is rejected, not guessed.
// X wraps the world (antimeridian). Y is clamped, never wrapped.
//
// Unique TileIds only: a tile that would appear twice under wrapping is
// returned once. Extremely wide views of a wrapped world are not an
// infinite scene graph -- see EnumerateVisibleTiles.
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

// Unique Web Mercator source tiles that intersect the viewport at
// viewport.zoom (no overzoom). Order: north-to-south, then west-to-east
// in screen space (wrapped X increases to the right).
//
// X indices wrap in [0, 2^zoom). Y indices outside [0, 2^zoom) are
// dropped, not wrapped. Returns false and clears `out` if `out` is null,
// zoom is invalid, or width/height/tile_size_px are <= 0.
//
// Does not consult map storage: a returned tile may be absent from a
// pack. Duplicate TileIds are never emitted (z0 + a wide viewport yields
// one 0/0/0, not five). A unique TileId is not instantiated at multiple
// world copies; that is out of scope for embedded viewports.
bool EnumerateVisibleTiles(const Viewport& viewport, std::vector<TileId>* out);

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
