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

bool SetCenter(Viewport* viewport, double lat_deg, double lon_deg);
LatLon GetCenter(const Viewport& viewport);
bool SetZoom(Viewport* viewport, int zoom);
uint8_t GetZoom(const Viewport& viewport);
bool ZoomIn(Viewport* viewport);
bool ZoomOut(Viewport* viewport);
bool SetViewportSize(Viewport* viewport, int width_px, int height_px);

// Positive dx/dy moves the camera east/south. Touch-drag bindings normally
// pass the negative gesture delta.
bool PanByPixels(Viewport* viewport, double dx, double dy);
bool ProjectLatLon(const Viewport& viewport, LatLon point, double* screen_x,
                   double* screen_y);
bool ScreenToLatLon(const Viewport& viewport, double screen_x,
                    double screen_y, LatLon* point);
bool GetVisibleBounds(const Viewport& viewport, GeoBounds* bounds);
// Centres on `bounds` and picks the LARGEST zoom at which all of `bounds`
// is visible inside the viewport (minus padding). Use this to show a pack's
// whole coverage area; the result usually leaves empty margin, because a
// pack's aspect ratio rarely matches the screen's.
bool FitBounds(Viewport* viewport, const GeoBounds& bounds, int padding_px);

// Centres on `bounds` and picks the SMALLEST zoom at which `bounds` fully
// covers the viewport -- the complement of FitBounds. Use this to fill the
// screen with map data instead of framing the coverage area, which is what
// a map application normally wants: correct centre, no empty margin, and a
// zoom derived from the caller's own width/height rather than a per-board
// constant.
//
// Returns false when no zoom up to the maximum can cover the viewport
// (bounds too small relative to the screen); callers should then fall back
// to FitBounds, which always succeeds for valid bounds.
bool FillBounds(Viewport* viewport, const GeoBounds& bounds);
bool ZoomAtScreenPoint(Viewport* viewport, double screen_x, double screen_y,
                       int zoom_delta);

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
