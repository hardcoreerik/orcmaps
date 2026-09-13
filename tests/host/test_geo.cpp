#include "orcmap/geo.hpp"

#include <cstdio>

#include "test_util.hpp"

namespace {

void TestWrapLongitude() {
  ORCMAP_EXPECT_NEAR(orcmap::WrapLongitudeDeg(0.0), 0.0, 1e-9);
  ORCMAP_EXPECT_NEAR(orcmap::WrapLongitudeDeg(180.0), -180.0, 1e-9);
  ORCMAP_EXPECT_NEAR(orcmap::WrapLongitudeDeg(190.0), -170.0, 1e-9);
  ORCMAP_EXPECT_NEAR(orcmap::WrapLongitudeDeg(-190.0), 170.0, 1e-9);
  ORCMAP_EXPECT_NEAR(orcmap::WrapLongitudeDeg(360.0), 0.0, 1e-9);
  ORCMAP_EXPECT_NEAR(orcmap::WrapLongitudeDeg(-360.0), 0.0, 1e-9);
}

void TestClampLatitude() {
  ORCMAP_EXPECT_NEAR(orcmap::ClampLatitudeDeg(0.0), 0.0, 1e-9);
  ORCMAP_EXPECT_NEAR(orcmap::ClampLatitudeDeg(90.0), orcmap::kMercatorMaxLatDeg,
                      1e-9);
  ORCMAP_EXPECT_NEAR(orcmap::ClampLatitudeDeg(-90.0),
                      -orcmap::kMercatorMaxLatDeg, 1e-9);
  ORCMAP_EXPECT_NEAR(orcmap::ClampLatitudeDeg(45.0), 45.0, 1e-9);
}

void TestTilesPerAxis() {
  ORCMAP_EXPECT_EQ(orcmap::TilesPerAxis(0), 1u);
  ORCMAP_EXPECT_EQ(orcmap::TilesPerAxis(1), 2u);
  ORCMAP_EXPECT_EQ(orcmap::TilesPerAxis(10), 1024u);
}

void TestKnownTiles() {
  // World corners at zoom 2 (n=4): NW corner (-180, ~85.05) is tile (0,0);
  // the equator/antimeridian point (0,-180) is tile (0, 2) (west edge,
  // vertically centered).
  const orcmap::TileId nw = orcmap::LatLonToTile(85.0, -180.0, 2);
  ORCMAP_EXPECT_EQ(nw.x, 0u);
  ORCMAP_EXPECT_EQ(nw.y, 0u);

  const orcmap::TileId equator_west = orcmap::LatLonToTile(0.0, -180.0, 2);
  ORCMAP_EXPECT_EQ(equator_west.x, 0u);
  ORCMAP_EXPECT_EQ(equator_west.y, 2u);
}

void TestAntimeridianWrap() {
  const orcmap::TileId a = orcmap::LatLonToTile(10.0, 185.0, 6);
  const orcmap::TileId b = orcmap::LatLonToTile(10.0, -175.0, 6);
  ORCMAP_EXPECT_EQ(a.x, b.x);
  ORCMAP_EXPECT_EQ(a.y, b.y);
}

void TestHighLatitudeClamp() {
  // Above the Mercator limit, tile math must clamp rather than produce
  // NaN/Inf or an out-of-range tile row.
  const orcmap::TileId near_pole = orcmap::LatLonToTile(89.9, 0.0, 5);
  ORCMAP_EXPECT_EQ(near_pole.y, 0u);
  const orcmap::TileId near_south_pole = orcmap::LatLonToTile(-89.9, 0.0, 5);
  ORCMAP_EXPECT_EQ(near_south_pole.y, orcmap::TilesPerAxis(5) - 1);
}

void TestRoundTripContainment() {
  // Eugene, OR -- the Lane County vertical-slice test region.
  const double lat = 44.0521;
  const double lon = -123.0868;
  for (uint8_t zoom = 0; zoom <= 16; ++zoom) {
    const orcmap::TileId tile = orcmap::LatLonToTile(lat, lon, zoom);
    const orcmap::LatLon nw = orcmap::TileToLatLon(tile);
    const orcmap::LatLon se =
        orcmap::TileToLatLon(orcmap::TileId{zoom, tile.x + 1, tile.y + 1});
    // North-west corner has the larger latitude and smaller longitude.
    ORCMAP_EXPECT_TRUE(lat <= nw.lat_deg + 1e-6);
    ORCMAP_EXPECT_TRUE(lat >= se.lat_deg - 1e-6);
    ORCMAP_EXPECT_TRUE(lon >= nw.lon_deg - 1e-6);
    ORCMAP_EXPECT_TRUE(lon <= se.lon_deg + 1e-6);
  }
}

}  // namespace

void RunGeoTests() {
  TestWrapLongitude();
  TestClampLatitude();
  TestTilesPerAxis();
  TestKnownTiles();
  TestAntimeridianWrap();
  TestHighLatitudeClamp();
  TestRoundTripContainment();
}
