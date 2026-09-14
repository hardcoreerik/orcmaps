#include "orcmap/viewport.hpp"

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

}  // namespace

void RunViewportTests() {
  TestTileOriginMapsToTopLeft();
  TestTileCenterMapsToScreenCenter();
  TestTileFarCornerMapsToExtent();
  TestPreparedTransformMatchesSimpleCases();
  TestPreparedTransformRejectsZoomMismatch();
  TestPreparedTransformRejectsInvalidZoom();
  TestNullScreenPointersAreSafe();
}
