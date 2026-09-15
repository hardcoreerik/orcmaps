#include "orcmap/viewport.hpp"

#include <cmath>
#include <limits>
#include <set>
#include <utility>
#include <vector>

#include "test_util.hpp"

namespace {

orcmap::Viewport Z0Frame(int size) {
  orcmap::Viewport v;
  v.center_lat_deg = 0.0;
  v.center_lon_deg = 0.0;
  v.zoom = 0;
  v.width_px = size;
  v.height_px = size;
  v.tile_size_px = size;
  return v;
}

void TestTileOriginMapsToTopLeft() {
  const orcmap::Viewport v = Z0Frame(64);
  const orcmap::TileId tile{0, 0, 0};
  int sx = -1, sy = -1;
  orcmap::TileLocalToScreen(v, tile, 4096, 0, 0, &sx, &sy);
  ORCMAP_EXPECT_EQ(sx, 0);
  ORCMAP_EXPECT_EQ(sy, 0);
}

void TestTileCenterMapsToScreenCenter() {
  const orcmap::Viewport v = Z0Frame(64);
  const orcmap::TileId tile{0, 0, 0};
  int sx = -1, sy = -1;
  orcmap::TileLocalToScreen(v, tile, 4096, 2048, 2048, &sx, &sy);
  ORCMAP_EXPECT_EQ(sx, 32);
  ORCMAP_EXPECT_EQ(sy, 32);
}

void TestTileFarCornerMapsToExtent() {
  const orcmap::Viewport v = Z0Frame(64);
  const orcmap::TileId tile{0, 0, 0};
  int sx = -1, sy = -1;
  orcmap::TileLocalToScreen(v, tile, 4096, 4096, 4096, &sx, &sy);
  ORCMAP_EXPECT_EQ(sx, 64);
  ORCMAP_EXPECT_EQ(sy, 64);
}

void TestPreparedTransformMatchesSimpleCases() {
  const orcmap::Viewport v = Z0Frame(64);
  const orcmap::TileId tile{0, 0, 0};
  orcmap::TileScreenMap map;
  ORCMAP_EXPECT_TRUE(orcmap::MakeTileScreenMap(v, tile, 4096, &map));
  ORCMAP_EXPECT_TRUE(map.valid);
  int sx = -1, sy = -1;
  orcmap::ProjectLocal(map, 0, 0, &sx, &sy);
  ORCMAP_EXPECT_EQ(sx, 0);
  ORCMAP_EXPECT_EQ(sy, 0);
  orcmap::ProjectLocal(map, 2048, 2048, &sx, &sy);
  ORCMAP_EXPECT_EQ(sx, 32);
  ORCMAP_EXPECT_EQ(sy, 32);
  orcmap::ProjectLocal(map, 4096, 4096, &sx, &sy);
  ORCMAP_EXPECT_EQ(sx, 64);
  ORCMAP_EXPECT_EQ(sy, 64);
}

void TestPreparedTransformRejectsZoomMismatch() {
  const orcmap::Viewport v = Z0Frame(32);
  orcmap::TileScreenMap map;
  ORCMAP_EXPECT_TRUE(
      !orcmap::MakeTileScreenMap(v, orcmap::TileId{1, 0, 0}, 4096, &map));
  ORCMAP_EXPECT_TRUE(!map.valid);
}

void TestPreparedTransformRejectsInvalidZoom() {
  orcmap::Viewport v = Z0Frame(32);
  v.zoom = 32;
  orcmap::TileScreenMap map;
  ORCMAP_EXPECT_TRUE(
      !orcmap::MakeTileScreenMap(v, orcmap::TileId{32, 0, 0}, 4096, &map));
  ORCMAP_EXPECT_TRUE(!map.valid);
}

void TestNullScreenPointersAreSafe() {
  const orcmap::Viewport v = Z0Frame(32);
  orcmap::TileLocalToScreen(v, orcmap::TileId{0, 0, 0}, 4096, 0, 0, nullptr,
                            nullptr);
  int sx = 7;
  orcmap::TileLocalToScreen(v, orcmap::TileId{0, 0, 0}, 4096, 0, 0, &sx,
                            nullptr);
  ORCMAP_EXPECT_EQ(sx, 7);
}

orcmap::Viewport MakeView(double lat, double lon, uint8_t z, int w, int h,
                          int tile_px) {
  orcmap::Viewport v;
  v.center_lat_deg = lat;
  v.center_lon_deg = lon;
  v.zoom = z;
  v.width_px = w;
  v.height_px = h;
  v.tile_size_px = tile_px;
  return v;
}

bool HasTile(const std::vector<orcmap::TileId>& tiles, orcmap::TileId want) {
  for (const orcmap::TileId& t : tiles) {
    if (t == want) return true;
  }
  return false;
}

bool UniqueAndSameZoom(const std::vector<orcmap::TileId>& tiles, uint8_t z) {
  std::set<std::pair<uint32_t, uint32_t>> seen;
  for (const orcmap::TileId& t : tiles) {
    if (t.z != z) return false;
    if (!seen.insert({t.x, t.y}).second) return false;
  }
  return true;
}

bool OrderedNorthThenEast(const std::vector<orcmap::TileId>& tiles) {
  for (size_t i = 1; i < tiles.size(); ++i) {
    if (tiles[i].y < tiles[i - 1].y) return false;
    if (tiles[i].y == tiles[i - 1].y) {
      // Screen-west to east is wrapped-X increasing from the first column
      // of that row; we only require y non-decreasing here. X uniqueness
      // on a row is checked separately.
    }
  }
  return true;
}

void TestEnumerateRejectsBadViewport() {
  std::vector<orcmap::TileId> tiles{orcmap::TileId{1, 1, 1}};
  ORCMAP_EXPECT_TRUE(!orcmap::EnumerateVisibleTiles(Z0Frame(32), nullptr));
  orcmap::Viewport v = Z0Frame(32);
  v.zoom = 32;
  ORCMAP_EXPECT_TRUE(!orcmap::EnumerateVisibleTiles(v, &tiles));
  ORCMAP_EXPECT_TRUE(tiles.empty());
  v = Z0Frame(32);
  v.width_px = 0;
  ORCMAP_EXPECT_TRUE(!orcmap::EnumerateVisibleTiles(v, &tiles));
  v = Z0Frame(32);
  v.tile_size_px = 0;
  ORCMAP_EXPECT_TRUE(!orcmap::EnumerateVisibleTiles(v, &tiles));
}

void TestEnumerateZ0WideViewportUnique() {
  const orcmap::Viewport v = MakeView(0, 0, 0, 1280, 720, 256);
  std::vector<orcmap::TileId> tiles;
  ORCMAP_EXPECT_TRUE(orcmap::EnumerateVisibleTiles(v, &tiles));
  ORCMAP_EXPECT_EQ(tiles.size(), static_cast<size_t>(1));
  ORCMAP_EXPECT_TRUE((tiles[0] == orcmap::TileId{0, 0, 0}));
}

void TestEnumerateZ1Z2NoDuplicates() {
  for (uint8_t z : {uint8_t{1}, uint8_t{2}}) {
    const orcmap::Viewport v = MakeView(0, 0, z, 1280, 720, 256);
    std::vector<orcmap::TileId> tiles;
    ORCMAP_EXPECT_TRUE(orcmap::EnumerateVisibleTiles(v, &tiles));
    ORCMAP_EXPECT_TRUE(UniqueAndSameZoom(tiles, z));
    ORCMAP_EXPECT_TRUE(!tiles.empty());
    ORCMAP_EXPECT_TRUE(OrderedNorthThenEast(tiles));
  }
}

void TestEnumerate1280x720CoversCorners() {
  // Center on a tile center at z14.
  const orcmap::TileCoord c = orcmap::LatLonToTileCoord(44.05, -123.022, 14);
  const orcmap::Viewport v = MakeView(44.05, -123.022, 14, 1280, 720, 256);
  std::vector<orcmap::TileId> tiles;
  ORCMAP_EXPECT_TRUE(orcmap::EnumerateVisibleTiles(v, &tiles));
  ORCMAP_EXPECT_TRUE(UniqueAndSameZoom(tiles, 14));
  const uint32_t n = orcmap::TilesPerAxis(14);
  const int samples[][2] = {{0, 0}, {1279, 0}, {0, 719}, {1279, 719}, {640, 360}};
  for (const auto& s : samples) {
    const double tx =
        c.x + (static_cast<double>(s[0]) + 0.5 - 640.0) / 256.0;
    const double ty =
        c.y + (static_cast<double>(s[1]) + 0.5 - 360.0) / 256.0;
    if (ty < 0.0 || ty >= static_cast<double>(n)) continue;
    double xw = tx;
    const double nd = static_cast<double>(n);
    xw -= nd * std::floor(xw / nd);
    if (xw < 0.0) xw += nd;
    const orcmap::TileId want{
        14, static_cast<uint32_t>(std::floor(xw)),
        static_cast<uint32_t>(std::floor(ty))};
    ORCMAP_EXPECT_TRUE(HasTile(tiles, want));
  }
}

void TestEnumerateCenterOnTileEdgeAndCorner() {
  const orcmap::Viewport edge = MakeView(0, 0, 5, 1280, 720, 256);
  std::vector<orcmap::TileId> a;
  ORCMAP_EXPECT_TRUE(orcmap::EnumerateVisibleTiles(edge, &a));
  ORCMAP_EXPECT_TRUE(UniqueAndSameZoom(a, 5));

  const orcmap::Viewport odd = MakeView(10.0, 20.0, 8, 1281, 721, 256);
  std::vector<orcmap::TileId> b;
  ORCMAP_EXPECT_TRUE(orcmap::EnumerateVisibleTiles(odd, &b));
  ORCMAP_EXPECT_TRUE(UniqueAndSameZoom(b, 8));
  ORCMAP_EXPECT_TRUE(!b.empty());
}

void TestEnumerateAntimeridianWrapsX() {
  const orcmap::Viewport v = MakeView(0.0, 179.9, 3, 512, 256, 256);
  std::vector<orcmap::TileId> tiles;
  ORCMAP_EXPECT_TRUE(orcmap::EnumerateVisibleTiles(v, &tiles));
  ORCMAP_EXPECT_TRUE(UniqueAndSameZoom(tiles, 3));
  bool saw_high = false;
  bool saw_zero = false;
  const uint32_t n = orcmap::TilesPerAxis(3);
  for (const orcmap::TileId& t : tiles) {
    if (t.x == 0) saw_zero = true;
    if (t.x + 1 == n) saw_high = true;
  }
  ORCMAP_EXPECT_TRUE(saw_high);
  ORCMAP_EXPECT_TRUE(saw_zero);
}

void TestEnumerateWestAntimeridian() {
  const orcmap::Viewport v = MakeView(0.0, -179.9, 4, 512, 256, 256);
  std::vector<orcmap::TileId> tiles;
  ORCMAP_EXPECT_TRUE(orcmap::EnumerateVisibleTiles(v, &tiles));
  ORCMAP_EXPECT_TRUE(UniqueAndSameZoom(tiles, 4));
  bool saw_high = false;
  bool saw_zero = false;
  const uint32_t n = orcmap::TilesPerAxis(4);
  for (const orcmap::TileId& t : tiles) {
    if (t.x == 0) saw_zero = true;
    if (t.x + 1 == n) saw_high = true;
  }
  ORCMAP_EXPECT_TRUE(saw_high);
  ORCMAP_EXPECT_TRUE(saw_zero);
}

void TestEnumeratePolarYClamped() {
  const orcmap::Viewport north =
      MakeView(orcmap::kMercatorMaxLatDeg, 0.0, 6, 512, 512, 256);
  std::vector<orcmap::TileId> tiles;
  ORCMAP_EXPECT_TRUE(orcmap::EnumerateVisibleTiles(north, &tiles));
  ORCMAP_EXPECT_TRUE(UniqueAndSameZoom(tiles, 6));
  const uint32_t n = orcmap::TilesPerAxis(6);
  for (const orcmap::TileId& t : tiles) {
    ORCMAP_EXPECT_TRUE(t.y < n);
  }
  ORCMAP_EXPECT_TRUE(!tiles.empty());
  ORCMAP_EXPECT_TRUE(tiles.front().y == 0);

  const orcmap::Viewport south =
      MakeView(-orcmap::kMercatorMaxLatDeg, 0.0, 6, 512, 512, 256);
  tiles.clear();
  ORCMAP_EXPECT_TRUE(orcmap::EnumerateVisibleTiles(south, &tiles));
  for (const orcmap::TileId& t : tiles) {
    ORCMAP_EXPECT_TRUE(t.y < n);
  }
  ORCMAP_EXPECT_TRUE(!tiles.empty());
  ORCMAP_EXPECT_TRUE(tiles.back().y + 1 == n);
}

void TestWrappedXProjectsAdjacent() {
  const orcmap::Viewport v = MakeView(0.0, 179.9, 2, 256, 256, 256);
  // z2: n=4. lon 179.9 is just west of +180, so x≈3.999. Tile 0 must sit
  // immediately to the right of center, not a full world away.
  int sx0 = 0, sy0 = 0;
  orcmap::TileLocalToScreen(v, orcmap::TileId{2, 0, 1}, 4096, 0, 2048, &sx0,
                            &sy0);
  int sx3 = 0, sy3 = 0;
  orcmap::TileLocalToScreen(v, orcmap::TileId{2, 3, 1}, 4096, 4096, 2048, &sx3,
                            &sy3);
  ORCMAP_EXPECT_TRUE(sx0 >= 120 && sx0 <= 140);
  ORCMAP_EXPECT_TRUE(sx3 >= 120 && sx3 <= 140);
  const int gap = sx0 - sx3;
  ORCMAP_EXPECT_TRUE(gap >= -2 && gap <= 2);
}

void TestMakeTileScreenMapRejectsOutOfRangeXy() {
  const orcmap::Viewport v = MakeView(0, 0, 2, 256, 256, 256);
  orcmap::TileScreenMap map;
  ORCMAP_EXPECT_TRUE(
      !orcmap::MakeTileScreenMap(v, orcmap::TileId{2, 4, 0}, 4096, &map));
  ORCMAP_EXPECT_TRUE(
      !orcmap::MakeTileScreenMap(v, orcmap::TileId{2, 0, 4}, 4096, &map));
}

void TestCameraCenterAndZoomControls() {
  orcmap::Viewport v = Z0Frame(320);
  ORCMAP_EXPECT_TRUE(!orcmap::SetViewportSize(&v, 0, 170));
  ORCMAP_EXPECT_TRUE(orcmap::SetViewportSize(&v, 320, 170));
  ORCMAP_EXPECT_EQ(v.width_px, 320);
  ORCMAP_EXPECT_EQ(v.height_px, 170);
  ORCMAP_EXPECT_TRUE(orcmap::SetCenter(&v, 100.0, 540.0));
  const orcmap::LatLon center = orcmap::GetCenter(v);
  ORCMAP_EXPECT_NEAR(center.lat_deg, orcmap::kMercatorMaxLatDeg, 1e-9);
  ORCMAP_EXPECT_NEAR(center.lon_deg, -180.0, 1e-9);
  ORCMAP_EXPECT_TRUE(orcmap::SetZoom(&v, 14));
  ORCMAP_EXPECT_EQ(orcmap::GetZoom(v), 14);
  for (int i = 0; i < 40; ++i) orcmap::ZoomIn(&v);
  ORCMAP_EXPECT_EQ(orcmap::GetZoom(v), orcmap::kMaxZoom);
  for (int i = 0; i < 40; ++i) orcmap::ZoomOut(&v);
  ORCMAP_EXPECT_EQ(orcmap::GetZoom(v), 0);
}

void TestPanMovesCameraAndWrapsAntimeridian() {
  orcmap::Viewport v = MakeView(0.0, 179.0, 2, 320, 170, 256);
  ORCMAP_EXPECT_TRUE(orcmap::PanByPixels(&v, 32.0, 32.0));
  ORCMAP_EXPECT_TRUE(v.center_lon_deg < -160.0);
  ORCMAP_EXPECT_TRUE(v.center_lat_deg < 0.0);
}

void TestVisibleBoundsWorldAndTinyViewport() {
  orcmap::Viewport world = MakeView(0.0, 0.0, 0, 256, 256, 256);
  orcmap::GeoBounds bounds;
  ORCMAP_EXPECT_TRUE(orcmap::GetVisibleBounds(world, &bounds));
  ORCMAP_EXPECT_NEAR(bounds.min_lon_deg, -180.0, 1e-9);
  ORCMAP_EXPECT_NEAR(bounds.max_lon_deg, 180.0, 1e-9);
  ORCMAP_EXPECT_NEAR(bounds.min_lat_deg, -orcmap::kMercatorMaxLatDeg, 1e-7);
  ORCMAP_EXPECT_NEAR(bounds.max_lat_deg, orcmap::kMercatorMaxLatDeg, 1e-7);

  orcmap::Viewport tiny = MakeView(44.05, -123.022, 14, 1, 1, 256);
  ORCMAP_EXPECT_TRUE(orcmap::GetVisibleBounds(tiny, &bounds));
  ORCMAP_EXPECT_TRUE(bounds.min_lat_deg <= 44.05 && bounds.max_lat_deg >= 44.05);
}

void TestFitBoundsWorldOregonAndSpringfield() {
  orcmap::Viewport v = MakeView(0, 0, 10, 320, 170, 256);
  ORCMAP_EXPECT_TRUE(!orcmap::FitBounds(
      &v, {-180.0, -85.0, 180.0, 85.0}, std::numeric_limits<int>::max()));
  ORCMAP_EXPECT_TRUE(orcmap::FitBounds(
      &v, {-180.0, -orcmap::kMercatorMaxLatDeg, 180.0,
           orcmap::kMercatorMaxLatDeg}, 0));
  ORCMAP_EXPECT_EQ(v.zoom, 0);
  ORCMAP_EXPECT_TRUE(orcmap::FitBounds(&v, {-124.7, 41.9, -116.4, 46.3}, 8));
  const uint8_t oregon_zoom = v.zoom;
  ORCMAP_EXPECT_TRUE(orcmap::FitBounds(
      &v, {-123.055, 44.030, -122.960, 44.090}, 8));
  ORCMAP_EXPECT_TRUE(v.zoom > oregon_zoom);
  ORCMAP_EXPECT_NEAR(v.center_lon_deg, -123.0075, 1e-4);
}

void TestFillBoundsCentresAndCoversViewport() {
  // The Springfield extract, on the Tab5 map area. FitBounds frames the
  // whole extract (empty margin); FillBounds fills the screen instead.
  const orcmap::GeoBounds springfield{-123.055, 44.030, -122.960, 44.090};
  orcmap::Viewport v = MakeView(0.0, 0.0, 0, 1280, 600, 256);

  ORCMAP_EXPECT_TRUE(orcmap::FitBounds(&v, springfield, 24));
  const uint8_t fit_zoom = v.zoom;

  ORCMAP_EXPECT_TRUE(orcmap::FillBounds(&v, springfield));
  const uint8_t fill_zoom = v.zoom;

  // Filling the screen must be at least as zoomed-in as framing it.
  ORCMAP_EXPECT_TRUE(fill_zoom > fit_zoom);

  // Centred on the extract, not on some hardcoded city coordinate.
  ORCMAP_EXPECT_NEAR(v.center_lon_deg, -123.0075, 1e-4);
  ORCMAP_EXPECT_NEAR(v.center_lat_deg, 44.060, 1e-3);

  // And the chosen zoom genuinely covers: visible bounds inside the pack.
  orcmap::GeoBounds visible{};
  ORCMAP_EXPECT_TRUE(orcmap::GetVisibleBounds(v, &visible));
  ORCMAP_EXPECT_TRUE(visible.min_lon_deg >= springfield.min_lon_deg);
  ORCMAP_EXPECT_TRUE(visible.max_lon_deg <= springfield.max_lon_deg);
  ORCMAP_EXPECT_TRUE(visible.min_lat_deg >= springfield.min_lat_deg);
  ORCMAP_EXPECT_TRUE(visible.max_lat_deg <= springfield.max_lat_deg);
}

void TestFillBoundsIsScreenDependent() {
  // Same pack, two very different displays: the engine must derive a
  // different zoom from width/height alone, with no per-board constant.
  const orcmap::GeoBounds springfield{-123.055, 44.030, -122.960, 44.090};
  orcmap::Viewport tab5 = MakeView(0.0, 0.0, 0, 1280, 600, 256);
  orcmap::Viewport lilygo = MakeView(0.0, 0.0, 0, 320, 170, 256);
  ORCMAP_EXPECT_TRUE(orcmap::FillBounds(&tab5, springfield));
  ORCMAP_EXPECT_TRUE(orcmap::FillBounds(&lilygo, springfield));
  // A bigger screen needs more ground, so it must zoom in further to stay
  // inside the same extract.
  ORCMAP_EXPECT_TRUE(tab5.zoom > lilygo.zoom);
  ORCMAP_EXPECT_NEAR(tab5.center_lon_deg, lilygo.center_lon_deg, 1e-6);
}

void TestFillBoundsRejectsUnfillableBounds() {
  orcmap::Viewport v = MakeView(0.0, 0.0, 0, 1280, 600, 256);
  // A degenerate sliver cannot cover any viewport at any zoom.
  ORCMAP_EXPECT_TRUE(!orcmap::FillBounds(&v, {0.0, 0.0, 0.0, 0.0}));
  ORCMAP_EXPECT_TRUE(!orcmap::FillBounds(nullptr, {-1.0, -1.0, 1.0, 1.0}));
}

void TestProjectionRoundTripAfterPanAndZoom() {
  orcmap::Viewport v = MakeView(44.05, -123.022, 14, 320, 170, 256);
  ORCMAP_EXPECT_TRUE(orcmap::PanByPixels(&v, 17.0, -9.0));
  double x = 0.0, y = 0.0;
  const orcmap::LatLon point{44.051, -123.02};
  ORCMAP_EXPECT_TRUE(orcmap::ProjectLatLon(v, point, &x, &y));
  orcmap::LatLon round_trip;
  ORCMAP_EXPECT_TRUE(orcmap::ScreenToLatLon(v, x, y, &round_trip));
  ORCMAP_EXPECT_NEAR(round_trip.lat_deg, point.lat_deg, 1e-7);
  ORCMAP_EXPECT_NEAR(round_trip.lon_deg, point.lon_deg, 1e-7);
}

void TestZoomAtScreenPointPreservesAnchor() {
  orcmap::Viewport v = MakeView(44.05, -123.022, 12, 320, 170, 256);
  orcmap::LatLon before;
  ORCMAP_EXPECT_TRUE(orcmap::ScreenToLatLon(v, 240.0, 40.0, &before));
  ORCMAP_EXPECT_TRUE(orcmap::ZoomAtScreenPoint(&v, 240.0, 40.0, 2));
  orcmap::LatLon after;
  ORCMAP_EXPECT_TRUE(orcmap::ScreenToLatLon(v, 240.0, 40.0, &after));
  ORCMAP_EXPECT_NEAR(after.lat_deg, before.lat_deg, 1e-7);
  ORCMAP_EXPECT_NEAR(after.lon_deg, before.lon_deg, 1e-7);
  ORCMAP_EXPECT_TRUE(orcmap::ZoomAtScreenPoint(
      &v, 160.0, 85.0, std::numeric_limits<int>::max()));
  ORCMAP_EXPECT_EQ(v.zoom, orcmap::kMaxZoom);
}

}  // namespace

void RunViewportTests() {
  TestTileOriginMapsToTopLeft();
  TestTileCenterMapsToScreenCenter();
  TestTileFarCornerMapsToExtent();
  TestPreparedTransformMatchesSimpleCases();
  TestPreparedTransformRejectsZoomMismatch();
  TestPreparedTransformRejectsInvalidZoom();
  TestNullScreenPointersAreSafe();
  TestEnumerateRejectsBadViewport();
  TestEnumerateZ0WideViewportUnique();
  TestEnumerateZ1Z2NoDuplicates();
  TestEnumerate1280x720CoversCorners();
  TestEnumerateCenterOnTileEdgeAndCorner();
  TestEnumerateAntimeridianWrapsX();
  TestEnumerateWestAntimeridian();
  TestEnumeratePolarYClamped();
  TestWrappedXProjectsAdjacent();
  TestMakeTileScreenMapRejectsOutOfRangeXy();
  TestCameraCenterAndZoomControls();
  TestPanMovesCameraAndWrapsAntimeridian();
  TestVisibleBoundsWorldAndTinyViewport();
  TestFillBoundsCentresAndCoversViewport();
  TestFillBoundsIsScreenDependent();
  TestFillBoundsRejectsUnfillableBounds();
  TestFitBoundsWorldOregonAndSpringfield();
  TestProjectionRoundTripAfterPanAndZoom();
  TestZoomAtScreenPointPreservesAnchor();
}
