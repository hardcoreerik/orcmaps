#pragma once

#include <cstdint>

#include "orcmap/geo.hpp"
#include "orcmap/viewport.hpp"

// A "choose your location" camera controller.
//
// The selected point IS the viewport centre, marked by a crosshair the host
// draws at a fixed screen position. There is no separate pin coordinate to
// keep in sync, so the pin cannot drift from the map, and zoom preserves the
// selection for free because zooming about the centre does not move it.
//
// This lives in OrcMaps rather than in a consuming application because it is
// entirely camera work -- every board that can draw a map can offer the same
// picker, and it is testable against the real Viewport instead of a copy.
//
// What it adds over driving Viewport directly:
//
//   - The zoom-out floor comes from MinFillZoom or WorldViewZoom, derived
//     from the caller's own width and height, so zooming out cannot strand
//     the user on a screen with empty margin beside the map.
//   - Gesture deltas are translated to camera motion in one place, so
//     "the map follows the finger" is not re-derived per board with the
//     sign wrong.

namespace orcmap {

struct LocationPickerLimits {
  // Zoom range of the map data actually installed. The picker never selects
  // a zoom outside this, because there would be nothing to draw.
  uint8_t data_min_zoom = 0;
  uint8_t data_max_zoom = 0;
};

class LocationPicker {
 public:
  // `bounds` is the coverage of the map data being picked on. Pass the whole
  // world for an overview pack. The starting zoom and the zoom-out floor are
  // both derived from `viewport`'s pixel size and these bounds -- not from a
  // caller-supplied constant, which is how a viewport ends up showing blank
  // margin at low zoom.
  //
  // Returns false if the viewport or bounds are invalid, or if the limits
  // leave no usable zoom.
  bool Begin(const Viewport& viewport, const GeoBounds& bounds,
             const LocationPickerLimits& limits);

  // Moves the map with the gesture: the camera travels the opposite way, so
  // content appears to follow the finger.
  bool DragByGesture(double gesture_dx_px, double gesture_dy_px);

  // Brings the geographic point under (screen_x, screen_y) to the crosshair.
  bool RecenterAtScreen(double screen_x, double screen_y);

  bool ZoomIn();
  bool ZoomOut();

  // Returns to the initial whole-coverage view and centre.
  bool Reset();

  // The selected point: the viewport centre.
  LatLon Selection() const;

  const Viewport& View() const { return viewport_; }
  uint8_t Zoom() const { return viewport_.zoom; }

  // Lowest zoom the picker will go to. Never below the installed data's
  // minimum, and never below the zoom at which the coverage fills the
  // screen.
  uint8_t MinZoom() const { return min_zoom_; }
  uint8_t MaxZoom() const { return max_zoom_; }

  bool CanZoomIn() const { return viewport_.zoom < max_zoom_; }
  bool CanZoomOut() const { return viewport_.zoom > min_zoom_; }

  // True once the selection has moved away from the opening view. A caller
  // can use this to require a deliberate choice rather than accepting
  // whatever the map happened to open on.
  bool Moved() const { return moved_; }

 private:
  bool ApplyZoom(int target);

  Viewport viewport_{};
  GeoBounds bounds_{};
  LatLon initial_center_{};
  uint8_t initial_zoom_ = 0;
  uint8_t min_zoom_ = 0;
  uint8_t max_zoom_ = 0;
  bool ready_ = false;
  bool moved_ = false;
};

// Chooses the opening zoom and the zoom-out floor for a picker.
//
// Exposed separately because it is the part worth testing directly and the
// part a caller may want for its own UI labelling. `bounds` spanning the
// whole world uses WorldViewZoom (height decides, because world-copy
// placement covers width); anything smaller uses MinFillZoom.
bool LocationPickerZoomRange(const Viewport& viewport, const GeoBounds& bounds,
                             const LocationPickerLimits& limits,
                             uint8_t* out_min_zoom, uint8_t* out_max_zoom,
                             uint8_t* out_initial_zoom);

}  // namespace orcmap
