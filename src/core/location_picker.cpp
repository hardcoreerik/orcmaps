#include "orcmap/location_picker.hpp"

#include <cmath>

namespace orcmap {
namespace {

// viewport.cpp and pack.cpp each keep a ValidBounds in an anonymous
// namespace, so neither is linkable from here. Rather than widen one of them
// into the public API for this, the picker validates what it actually needs:
// finite, within Mercator limits, and non-degenerate.
bool UsableBounds(const GeoBounds& bounds) {
  if (!std::isfinite(bounds.min_lon_deg) || !std::isfinite(bounds.max_lon_deg) ||
      !std::isfinite(bounds.min_lat_deg) || !std::isfinite(bounds.max_lat_deg)) {
    return false;
  }
  if (bounds.min_lon_deg < -180.0 || bounds.max_lon_deg > 180.0) return false;
  if (bounds.min_lat_deg < -kMercatorMaxLatDeg ||
      bounds.max_lat_deg > kMercatorMaxLatDeg) {
    return false;
  }
  return bounds.min_lon_deg < bounds.max_lon_deg &&
         bounds.min_lat_deg < bounds.max_lat_deg;
}

// Same reason as UsableBounds: viewport.cpp's ValidViewport is not linkable.
//
// This has to be checked up front rather than relied on indirectly. The
// zoom-range fallback treats "no zoom can fill the screen" as a legitimate
// outcome, which would silently absorb an invalid viewport and open a picker
// on a 0x0 screen.
bool UsableViewport(const Viewport& viewport) {
  return viewport.width_px > 0 && viewport.height_px > 0 &&
         viewport.tile_size_px > 0 && ZoomIsValid(viewport.zoom) &&
         std::isfinite(viewport.center_lat_deg) &&
         std::isfinite(viewport.center_lon_deg);
}

bool ValidLimits(const LocationPickerLimits& limits) {
  return ZoomIsValid(limits.data_min_zoom) && ZoomIsValid(limits.data_max_zoom) &&
         limits.data_min_zoom <= limits.data_max_zoom;
}

uint8_t Clamp(int value, uint8_t low, uint8_t high) {
  if (value < static_cast<int>(low)) return low;
  if (value > static_cast<int>(high)) return high;
  return static_cast<uint8_t>(value);
}

}  // namespace

bool LocationPickerZoomRange(const Viewport& viewport, const GeoBounds& bounds,
                             const LocationPickerLimits& limits,
                             uint8_t* out_min_zoom, uint8_t* out_max_zoom,
                             uint8_t* out_initial_zoom) {
  if (out_min_zoom == nullptr || out_max_zoom == nullptr ||
      out_initial_zoom == nullptr) {
    return false;
  }
  if (!ValidLimits(limits) || !UsableViewport(viewport)) return false;

  // Whole-world coverage is the common case for a picker and needs the
  // height-only test: with world-copy placement the map repeats around
  // itself horizontally, so width is always covered and only height can
  // leave the screen short.
  const bool spans_world = (bounds.max_lon_deg - bounds.min_lon_deg) >= 360.0;
  uint8_t fill_zoom = 0;
  const bool have_fill = spans_world
                             ? WorldViewZoom(viewport, &fill_zoom)
                             : MinFillZoom(viewport, bounds, &fill_zoom);

  // If the coverage can never fill this screen, the data's own minimum is the
  // only floor available. The caller will see empty margin at that zoom, but
  // refusing to open the picker at all would be worse.
  const uint8_t floor_zoom =
      have_fill ? Clamp(fill_zoom, limits.data_min_zoom, limits.data_max_zoom)
                : limits.data_min_zoom;

  *out_min_zoom = floor_zoom;
  *out_max_zoom = limits.data_max_zoom;
  // Open at the floor: the widest view the screen can show without gaps,
  // which is where a user orients themselves before zooming in.
  *out_initial_zoom = floor_zoom;
  return true;
}

bool LocationPicker::Begin(const Viewport& viewport, const GeoBounds& bounds,
                           const LocationPickerLimits& limits) {
  ready_ = false;
  moved_ = false;
  if (!UsableBounds(bounds) || !UsableViewport(viewport)) return false;

  Viewport candidate = viewport;
  uint8_t min_zoom = 0;
  uint8_t max_zoom = 0;
  uint8_t initial_zoom = 0;
  if (!LocationPickerZoomRange(candidate, bounds, limits, &min_zoom, &max_zoom,
                               &initial_zoom)) {
    return false;
  }

  // Centre of the coverage, which for a world pack is 0,0 and for a regional
  // pack is the middle of the region -- not a hard-coded origin that would
  // open a regional picker on empty ocean.
  const double center_lat = (bounds.min_lat_deg + bounds.max_lat_deg) / 2.0;
  const double center_lon = (bounds.min_lon_deg + bounds.max_lon_deg) / 2.0;
  if (!SetZoom(&candidate, initial_zoom)) return false;
  if (!SetCenter(&candidate, center_lat, center_lon)) return false;

  viewport_ = candidate;
  bounds_ = bounds;
  min_zoom_ = min_zoom;
  max_zoom_ = max_zoom;
  initial_zoom_ = initial_zoom;
  initial_center_ = GetCenter(viewport_);
  ready_ = true;
  return true;
}

bool LocationPicker::DragByGesture(double gesture_dx_px, double gesture_dy_px) {
  if (!ready_) return false;
  // The map moves with the finger, so the camera moves the opposite way.
  if (!PanByPixels(&viewport_, -gesture_dx_px, -gesture_dy_px)) return false;
  moved_ = true;
  return true;
}

bool LocationPicker::RecenterAtScreen(double screen_x, double screen_y) {
  if (!ready_) return false;
  LatLon point{};
  if (!ScreenToLatLon(viewport_, screen_x, screen_y, &point)) return false;
  if (!SetCenter(&viewport_, point.lat_deg, point.lon_deg)) return false;
  moved_ = true;
  return true;
}

bool LocationPicker::ApplyZoom(int target) {
  if (!ready_) return false;
  const uint8_t clamped = Clamp(target, min_zoom_, max_zoom_);
  if (clamped == viewport_.zoom) return false;
  // SetZoom keeps the centre, which is the selection. Deliberately NOT
  // ZoomAtScreenPoint: anchoring on a screen point would move the crosshair's
  // own coordinate, so the act of zooming in to check a choice would change
  // it.
  return SetZoom(&viewport_, clamped);
}

bool LocationPicker::ZoomIn() {
  return ApplyZoom(static_cast<int>(viewport_.zoom) + 1);
}

bool LocationPicker::ZoomOut() {
  return ApplyZoom(static_cast<int>(viewport_.zoom) - 1);
}

bool LocationPicker::Reset() {
  if (!ready_) return false;
  if (!SetZoom(&viewport_, initial_zoom_)) return false;
  if (!SetCenter(&viewport_, initial_center_.lat_deg, initial_center_.lon_deg)) {
    return false;
  }
  moved_ = false;
  return true;
}

LatLon LocationPicker::Selection() const { return GetCenter(viewport_); }

}  // namespace orcmap
