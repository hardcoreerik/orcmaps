#include "orcmap/geo.hpp"

#include <algorithm>
#include <cmath>

namespace orcmap {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;
}  // namespace

double WrapLongitudeDeg(double lon_deg) {
  double wrapped = std::fmod(lon_deg + 180.0, 360.0);
  if (wrapped < 0.0) wrapped += 360.0;
  return wrapped - 180.0;
}

double ClampLatitudeDeg(double lat_deg) {
  return std::clamp(lat_deg, -kMercatorMaxLatDeg, kMercatorMaxLatDeg);
}

TileCoord LatLonToTileCoord(double lat_deg, double lon_deg, uint8_t zoom) {
  if (!ZoomIsValid(zoom)) return TileCoord{0.0, 0.0};
  const double n = static_cast<double>(TilesPerAxis(zoom));
  const double lon_wrapped = WrapLongitudeDeg(lon_deg);
  const double lat_clamped = ClampLatitudeDeg(lat_deg);
  const double lat_rad = lat_clamped * kDegToRad;

  const double x = (lon_wrapped + 180.0) / 360.0 * n;
  const double y =
      (1.0 - std::asinh(std::tan(lat_rad)) / kPi) / 2.0 * n;
  return TileCoord{x, y};
}

LatLon TileCoordToLatLon(TileCoord coord, uint8_t zoom) {
  if (!ZoomIsValid(zoom) || !std::isfinite(coord.x) ||
      !std::isfinite(coord.y)) {
    return LatLon{0.0, 0.0};
  }
  const double n = static_cast<double>(TilesPerAxis(zoom));
  double x = std::fmod(coord.x, n);
  if (x < 0.0) x += n;
  const double y = std::clamp(coord.y, 0.0, n);
  const double lon_deg = x / n * 360.0 - 180.0;
  const double lat_rad =
      std::atan(std::sinh(kPi * (1.0 - 2.0 * (y / n))));
  return LatLon{lat_rad * kRadToDeg, lon_deg};
}

TileId LatLonToTile(double lat_deg, double lon_deg, uint8_t zoom) {
  if (!ZoomIsValid(zoom)) return TileId{zoom, 0, 0};
  const TileCoord coord = LatLonToTileCoord(lat_deg, lon_deg, zoom);
  const double n = static_cast<double>(TilesPerAxis(zoom));
  // Clamp in floating point *before* casting to unsigned: floating-point
  // error at the Mercator latitude limit or the +180 deg antimeridian can
  // floor to -1 or n, and casting a negative double to uint32_t is
  // undefined/wraps to a huge value rather than saturating -- clamp first.
  const double x_clamped = std::clamp(std::floor(coord.x), 0.0, n - 1.0);
  const double y_clamped = std::clamp(std::floor(coord.y), 0.0, n - 1.0);
  return TileId{zoom, static_cast<uint32_t>(x_clamped),
                static_cast<uint32_t>(y_clamped)};
}

LatLon TileToLatLon(TileId tile) {
  if (!ZoomIsValid(tile.z)) return LatLon{0.0, 0.0};
  const double n = static_cast<double>(TilesPerAxis(tile.z));
  const double lon_deg = static_cast<double>(tile.x) / n * 360.0 - 180.0;
  const double y = std::clamp(static_cast<double>(tile.y), 0.0, n);
  const double lat_rad =
      std::atan(std::sinh(kPi * (1.0 - 2.0 * (y / n))));
  return LatLon{lat_rad * kRadToDeg, lon_deg};
}

uint32_t TilesPerAxis(uint8_t zoom) {
  if (!ZoomIsValid(zoom)) return 0u;
  return 1u << zoom;
}

}  // namespace orcmap
