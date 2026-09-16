#include <cmath>

#include "orcmap/location_picker.hpp"
#include "test_util.hpp"

namespace {

using orcmap::GeoBounds;
using orcmap::LatLon;
using orcmap::LocationPicker;
using orcmap::LocationPickerLimits;
using orcmap::Viewport;

// The Tab5's map area in the setup wizard: 1280x720 with a control panel on
// the right.
Viewport Tab5Viewport() {
  Viewport viewport;
  viewport.width_px = 930;
  viewport.height_px = 720;
  viewport.tile_size_px = 256;
  return viewport;
}

// The CYD 3.5" in landscape -- a very different aspect ratio, which is the
// point: the picker must derive its zooms rather than assume a screen.
Viewport CydViewport() {
  Viewport viewport;
  viewport.width_px = 480;
  viewport.height_px = 320;
  viewport.tile_size_px = 256;
  return viewport;
}

GeoBounds WorldBounds() {
  GeoBounds bounds;
  bounds.min_lon_deg = -180.0;
  bounds.max_lon_deg = 180.0;
  bounds.min_lat_deg = -orcmap::kMercatorMaxLatDeg;
  bounds.max_lat_deg = orcmap::kMercatorMaxLatDeg;
  return bounds;
}

// The provisioned Eugene pack from the OrcMaps pack evidence.
GeoBounds EugeneBounds() {
  GeoBounds bounds;
  bounds.min_lon_deg = -123.71644;
  bounds.max_lon_deg = -122.45696;
  bounds.min_lat_deg = 43.60294;
  bounds.max_lat_deg = 44.50126;
  return bounds;
}

LocationPickerLimits WorldPackLimits() {
  LocationPickerLimits limits;
  limits.data_min_zoom = 1;
  limits.data_max_zoom = 7;
  return limits;
}

void TestOpensAtAFillingZoom() {
  // The bug this exists to prevent: opening at a zoom where the world is
  // narrower or shorter than the screen, leaving blank margin.
  LocationPicker picker;
  ORCMAP_EXPECT_TRUE(picker.Begin(Tab5Viewport(), WorldBounds(), WorldPackLimits()));

  const int world_px = 256 << picker.Zoom();
  ORCMAP_EXPECT_TRUE(world_px >= 720);  // covers the height it opened on

  // One zoom lower must NOT cover it, or the opening zoom was not minimal.
  if (picker.Zoom() > picker.MinZoom() || picker.Zoom() > 1) {
    const int smaller_px = 256 << (picker.Zoom() - 1);
    ORCMAP_EXPECT_TRUE(smaller_px < 720);
  }
}

void TestZoomFloorDependsOnTheScreen() {
  // A 720px-tall viewport and a 320px-tall one must not get the same floor
  // from the same data; that is exactly what a hard-coded constant does.
  LocationPicker tall;
  LocationPicker shortish;
  ORCMAP_EXPECT_TRUE(tall.Begin(Tab5Viewport(), WorldBounds(), WorldPackLimits()));
  ORCMAP_EXPECT_TRUE(shortish.Begin(CydViewport(), WorldBounds(), WorldPackLimits()));
  ORCMAP_EXPECT_TRUE(tall.MinZoom() > shortish.MinZoom());
}

void TestCannotZoomBelowTheFloor() {
  LocationPicker picker;
  ORCMAP_EXPECT_TRUE(picker.Begin(Tab5Viewport(), WorldBounds(), WorldPackLimits()));
  const uint8_t floor = picker.MinZoom();
  ORCMAP_EXPECT_TRUE(!picker.CanZoomOut());
  for (int i = 0; i < 10; ++i) picker.ZoomOut();
  ORCMAP_EXPECT_TRUE(picker.Zoom() == floor);

  // And never above the installed data's maximum.
  for (int i = 0; i < 20; ++i) picker.ZoomIn();
  ORCMAP_EXPECT_TRUE(picker.Zoom() == WorldPackLimits().data_max_zoom);
  ORCMAP_EXPECT_TRUE(!picker.CanZoomIn());
}

void TestZoomPreservesTheSelection() {
  // THE invariant of a centre-crosshair picker, and the one the earlier
  // prototype never asserted: if zooming moved the selection, the act of
  // zooming in to check a choice would silently change it.
  LocationPicker picker;
  ORCMAP_EXPECT_TRUE(picker.Begin(Tab5Viewport(), WorldBounds(), WorldPackLimits()));
  ORCMAP_EXPECT_TRUE(picker.RecenterAtScreen(700.0, 300.0));
  const LatLon chosen = picker.Selection();

  for (int i = 0; i < 5; ++i) {
    if (!picker.ZoomIn()) break;
    const LatLon now = picker.Selection();
    ORCMAP_EXPECT_NEAR(now.lat_deg, chosen.lat_deg, 1e-9);
    ORCMAP_EXPECT_NEAR(now.lon_deg, chosen.lon_deg, 1e-9);
  }
  while (picker.ZoomOut()) {
    const LatLon now = picker.Selection();
    ORCMAP_EXPECT_NEAR(now.lat_deg, chosen.lat_deg, 1e-9);
    ORCMAP_EXPECT_NEAR(now.lon_deg, chosen.lon_deg, 1e-9);
  }
}

void TestDragMovesTheMapWithTheFinger() {
  LocationPicker picker;
  ORCMAP_EXPECT_TRUE(picker.Begin(Tab5Viewport(), WorldBounds(), WorldPackLimits()));
  ORCMAP_EXPECT_TRUE(picker.Zoom() == 2);  // pinned: 256<<2 = 1024 >= 720, 512 < 720

  const LatLon before = picker.Selection();
  // Dragging the map to the right reveals what is to the WEST, so the
  // selection's longitude must decrease.
  ORCMAP_EXPECT_TRUE(picker.DragByGesture(128.0, 0.0));
  const LatLon after = picker.Selection();
  ORCMAP_EXPECT_TRUE(after.lon_deg < before.lon_deg);

  // And by an exact amount: 128px of 1024px of world is one eighth, 45
  // degrees. A directional check would pass with the scale wrong.
  ORCMAP_EXPECT_NEAR(before.lon_deg - after.lon_deg, 45.0, 1e-9);
}

void TestDragVerticalDirection() {
  LocationPicker picker;
  ORCMAP_EXPECT_TRUE(picker.Begin(Tab5Viewport(), WorldBounds(), WorldPackLimits()));
  const LatLon before = picker.Selection();
  // Dragging the map down reveals what is to the NORTH.
  ORCMAP_EXPECT_TRUE(picker.DragByGesture(0.0, 100.0));
  ORCMAP_EXPECT_TRUE(picker.Selection().lat_deg > before.lat_deg);
}

void TestRecenterBringsTheTappedPointToTheCrosshair() {
  LocationPicker picker;
  ORCMAP_EXPECT_TRUE(picker.Begin(Tab5Viewport(), WorldBounds(), WorldPackLimits()));

  const double tap_x = 700.0;
  const double tap_y = 250.0;
  LatLon tapped{};
  ORCMAP_EXPECT_TRUE(orcmap::ScreenToLatLon(picker.View(), tap_x, tap_y, &tapped));
  ORCMAP_EXPECT_TRUE(picker.RecenterAtScreen(tap_x, tap_y));

  const LatLon selection = picker.Selection();
  ORCMAP_EXPECT_NEAR(selection.lat_deg, tapped.lat_deg, 1e-9);
  ORCMAP_EXPECT_NEAR(selection.lon_deg, tapped.lon_deg, 1e-9);

  // The chosen point is now under the crosshair, i.e. the viewport centre.
  double sx = 0.0;
  double sy = 0.0;
  ORCMAP_EXPECT_TRUE(orcmap::ProjectLatLon(picker.View(), selection, &sx, &sy));
  ORCMAP_EXPECT_NEAR(sx, 930.0 / 2.0, 1e-6);
  ORCMAP_EXPECT_NEAR(sy, 720.0 / 2.0, 1e-6);
}

void TestRegionalPackOpensOnItsOwnRegion() {
  // A regional picker must not open at 0,0 -- that is the Gulf of Guinea,
  // thousands of km outside the pack.
  LocationPickerLimits limits;
  limits.data_min_zoom = 1;
  limits.data_max_zoom = 13;

  LocationPicker picker;
  const GeoBounds bounds = EugeneBounds();
  ORCMAP_EXPECT_TRUE(picker.Begin(Tab5Viewport(), bounds, limits));
  const LatLon selection = picker.Selection();
  ORCMAP_EXPECT_TRUE(selection.lat_deg > bounds.min_lat_deg);
  ORCMAP_EXPECT_TRUE(selection.lat_deg < bounds.max_lat_deg);
  ORCMAP_EXPECT_TRUE(selection.lon_deg > bounds.min_lon_deg);
  ORCMAP_EXPECT_TRUE(selection.lon_deg < bounds.max_lon_deg);

  // A small region needs a HIGH zoom to fill a screen, so the floor must be
  // well above a world pack's.
  ORCMAP_EXPECT_TRUE(picker.MinZoom() > 7);
}

void TestMovedTracksDeliberateChoice() {
  LocationPicker picker;
  ORCMAP_EXPECT_TRUE(picker.Begin(Tab5Viewport(), WorldBounds(), WorldPackLimits()));
  ORCMAP_EXPECT_TRUE(!picker.Moved());
  ORCMAP_EXPECT_TRUE(picker.ZoomIn());
  // Zooming alone is not choosing a place.
  ORCMAP_EXPECT_TRUE(!picker.Moved());
  ORCMAP_EXPECT_TRUE(picker.DragByGesture(40.0, 0.0));
  ORCMAP_EXPECT_TRUE(picker.Moved());
  ORCMAP_EXPECT_TRUE(picker.Reset());
  ORCMAP_EXPECT_TRUE(!picker.Moved());
}

void TestResetReturnsToTheOpeningView() {
  LocationPicker picker;
  ORCMAP_EXPECT_TRUE(picker.Begin(Tab5Viewport(), WorldBounds(), WorldPackLimits()));
  const LatLon opening = picker.Selection();
  const uint8_t opening_zoom = picker.Zoom();

  ORCMAP_EXPECT_TRUE(picker.RecenterAtScreen(800.0, 600.0));
  picker.ZoomIn();
  picker.ZoomIn();
  ORCMAP_EXPECT_TRUE(picker.Reset());

  ORCMAP_EXPECT_TRUE(picker.Zoom() == opening_zoom);
  ORCMAP_EXPECT_NEAR(picker.Selection().lat_deg, opening.lat_deg, 1e-9);
  ORCMAP_EXPECT_NEAR(picker.Selection().lon_deg, opening.lon_deg, 1e-9);
}

void TestPanningAcrossTheAntimeridianStaysValid() {
  // A picker on a world map can be dragged past +/-180 indefinitely; the
  // selection must remain a real coordinate rather than running off.
  LocationPicker picker;
  ORCMAP_EXPECT_TRUE(picker.Begin(Tab5Viewport(), WorldBounds(), WorldPackLimits()));
  for (int i = 0; i < 40; ++i) {
    ORCMAP_EXPECT_TRUE(picker.DragByGesture(200.0, 0.0));
    const LatLon selection = picker.Selection();
    ORCMAP_EXPECT_TRUE(selection.lon_deg >= -180.0);
    ORCMAP_EXPECT_TRUE(selection.lon_deg <= 180.0);
    ORCMAP_EXPECT_TRUE(std::isfinite(selection.lat_deg));
  }
}

void TestPanningToThePolesIsClamped() {
  LocationPicker picker;
  ORCMAP_EXPECT_TRUE(picker.Begin(Tab5Viewport(), WorldBounds(), WorldPackLimits()));
  for (int i = 0; i < 40; ++i) picker.DragByGesture(0.0, 400.0);
  ORCMAP_EXPECT_TRUE(picker.Selection().lat_deg <= orcmap::kMercatorMaxLatDeg);
  for (int i = 0; i < 80; ++i) picker.DragByGesture(0.0, -400.0);
  ORCMAP_EXPECT_TRUE(picker.Selection().lat_deg >= -orcmap::kMercatorMaxLatDeg);
}

void TestUnstartedPickerRefusesInteraction() {
  LocationPicker picker;
  ORCMAP_EXPECT_TRUE(!picker.DragByGesture(10.0, 0.0));
  ORCMAP_EXPECT_TRUE(!picker.RecenterAtScreen(10.0, 10.0));
  ORCMAP_EXPECT_TRUE(!picker.ZoomIn());
  ORCMAP_EXPECT_TRUE(!picker.ZoomOut());
  ORCMAP_EXPECT_TRUE(!picker.Reset());
}

void TestInvalidInputIsRejected() {
  LocationPicker picker;
  GeoBounds degenerate = WorldBounds();
  degenerate.max_lat_deg = degenerate.min_lat_deg;
  ORCMAP_EXPECT_TRUE(!picker.Begin(Tab5Viewport(), degenerate, WorldPackLimits()));

  GeoBounds out_of_range = WorldBounds();
  out_of_range.max_lon_deg = 200.0;
  ORCMAP_EXPECT_TRUE(!picker.Begin(Tab5Viewport(), out_of_range, WorldPackLimits()));

  LocationPickerLimits inverted;
  inverted.data_min_zoom = 9;
  inverted.data_max_zoom = 3;
  ORCMAP_EXPECT_TRUE(!picker.Begin(Tab5Viewport(), WorldBounds(), inverted));

  Viewport zero;
  zero.width_px = 0;
  zero.height_px = 0;
  zero.tile_size_px = 256;
  ORCMAP_EXPECT_TRUE(!picker.Begin(zero, WorldBounds(), WorldPackLimits()));
}

void TestZoomRangeClampsToInstalledData() {
  // A pack that stops at z4 must not let the picker open at z5 even if the
  // screen would need it, because there would be no tiles to draw.
  LocationPickerLimits shallow;
  shallow.data_min_zoom = 1;
  shallow.data_max_zoom = 4;

  uint8_t min_zoom = 0;
  uint8_t max_zoom = 0;
  uint8_t initial = 0;
  ORCMAP_EXPECT_TRUE(orcmap::LocationPickerZoomRange(Tab5Viewport(), EugeneBounds(), shallow,
                                        &min_zoom, &max_zoom, &initial));
  ORCMAP_EXPECT_TRUE(max_zoom == 4);
  ORCMAP_EXPECT_TRUE(min_zoom <= 4);
  ORCMAP_EXPECT_TRUE(initial <= 4);
  ORCMAP_EXPECT_TRUE(initial >= 1);
}

void TestWorldAndRegionalUseDifferentZoomTests() {
  // World coverage is judged on height alone (width wraps); a region is
  // judged on both. The two must therefore disagree for the same screen.
  uint8_t world_min = 0;
  uint8_t world_max = 0;
  uint8_t world_initial = 0;
  LocationPickerLimits wide;
  wide.data_min_zoom = 0;
  wide.data_max_zoom = 20;
  ORCMAP_EXPECT_TRUE(orcmap::LocationPickerZoomRange(Tab5Viewport(), WorldBounds(), wide,
                                        &world_min, &world_max, &world_initial));

  uint8_t region_min = 0;
  uint8_t region_max = 0;
  uint8_t region_initial = 0;
  ORCMAP_EXPECT_TRUE(orcmap::LocationPickerZoomRange(Tab5Viewport(), EugeneBounds(), wide,
                                        &region_min, &region_max,
                                        &region_initial));
  ORCMAP_EXPECT_TRUE(region_initial > world_initial);
}

}  // namespace

void RunLocationPickerTests() {
  TestOpensAtAFillingZoom();
  TestZoomFloorDependsOnTheScreen();
  TestCannotZoomBelowTheFloor();
  TestZoomPreservesTheSelection();
  TestDragMovesTheMapWithTheFinger();
  TestDragVerticalDirection();
  TestRecenterBringsTheTappedPointToTheCrosshair();
  TestRegionalPackOpensOnItsOwnRegion();
  TestMovedTracksDeliberateChoice();
  TestResetReturnsToTheOpeningView();
  TestPanningAcrossTheAntimeridianStaysValid();
  TestPanningToThePolesIsClamped();
  TestUnstartedPickerRefusesInteraction();
  TestInvalidInputIsRejected();
  TestZoomRangeClampsToInstalledData();
  TestWorldAndRegionalUseDifferentZoomTests();
}
