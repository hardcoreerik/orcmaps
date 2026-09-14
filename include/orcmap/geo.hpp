#pragma once

#include <cstdint>

namespace orcmap {

// Standard Web Mercator (EPSG:3857) tile math -- the projection for the
// tiled basemap. This is intentionally separate from any RF/application-
// side geodesic math (range, bearing, great-circle distance), which stays
// in the consuming application. See docs/ARCHITECTURE.md.

// The Mercator projection is undefined at the poles; standard web-map
// tiling clamps latitude to this bound (where the projected square becomes
// exactly square, x and y spans equal).
inline constexpr double kMercatorMaxLatDeg = 85.05112878;

struct LatLon {
  double lat_deg;
  double lon_deg;
};

struct TileId {
  uint8_t z;
  uint32_t x;
  uint32_t y;
};

// Fractional tile-space coordinates at a given zoom: integer part is the
// tile index, fractional part is the position within that tile (0..1).
// Kept as one type so pixel-within-tile can be derived without a second
// projection call.
struct TileCoord {
  double x;
  double y;
};

// Wraps a longitude in degrees into [-180, 180).
double WrapLongitudeDeg(double lon_deg);

// Clamps a latitude in degrees into [-kMercatorMaxLatDeg, kMercatorMaxLatDeg].
double ClampLatitudeDeg(double lat_deg);

// Converts geographic coordinates to fractional tile coordinates at `zoom`.
// Longitude is wrapped and latitude is clamped internally, so this is
// always well-defined (no NaN/Inf) for any finite input.
TileCoord LatLonToTileCoord(double lat_deg, double lon_deg, uint8_t zoom);

// The integer tile (floor of LatLonToTileCoord) containing a point.
TileId LatLonToTile(double lat_deg, double lon_deg, uint8_t zoom);

// Inverse projection: the geographic coordinates of a tile's northwest
// (top-left) corner.
LatLon TileToLatLon(TileId tile);

// Web Mercator zoom is defined for 0..31 inclusive (2^31 tiles per axis
// still fits in uint32). zoom >= 32 is invalid: TilesPerAxis returns 0
// and the other helpers return deterministic zeros rather than dividing
// by zero.
inline constexpr uint8_t kMaxZoom = 31;

inline bool ZoomIsValid(uint8_t zoom) { return zoom <= kMaxZoom; }

// Number of tiles per axis at a zoom level (2^zoom). Returns 0 if zoom
// is not valid (zoom > 31).
uint32_t TilesPerAxis(uint8_t zoom);

}  // namespace orcmap
