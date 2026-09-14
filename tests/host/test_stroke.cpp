#include "framebuffer_target.hpp"
#include "orcmap/clip.hpp"

#include <limits>
#include "orcmap/feature.hpp"
#include "orcmap/renderer.hpp"
#include "orcmap/stroke.hpp"
#include "orcmap/style.hpp"
#include "orcmap/viewport.hpp"

#include "test_util.hpp"

namespace {

orcmap::Viewport Z0(int px) {
  orcmap::Viewport vp;
  vp.zoom = 0;
  vp.width_px = px;
  vp.height_px = px;
  vp.tile_size_px = px;
  return vp;
}

int CountColor(const orcmap::host::FramebufferTarget& fb, orcmap::Color c) {
  int n = 0;
  for (int y = 0; y < fb.Height(); ++y) {
    for (int x = 0; x < fb.Width(); ++x) {
      if (fb.At(x, y) == c) ++n;
    }
  }
  return n;
}

void TestRasterWidthZeroNegative() {
  ORCMAP_EXPECT_EQ(orcmap::RasterStrokeWidthPx(0.0f), 0);
  ORCMAP_EXPECT_EQ(orcmap::RasterStrokeWidthPx(-1.0f), 0);
  ORCMAP_EXPECT_EQ(
      orcmap::RasterStrokeWidthPx(std::numeric_limits<float>::quiet_NaN()), 0);
}

void TestRasterWidthOneAndBelow() {
  ORCMAP_EXPECT_EQ(orcmap::RasterStrokeWidthPx(0.25f), 1);
  ORCMAP_EXPECT_EQ(orcmap::RasterStrokeWidthPx(1.0f), 1);
}

void TestRasterWidthFractional() {
  ORCMAP_EXPECT_EQ(orcmap::RasterStrokeWidthPx(1.8f), 2);
  ORCMAP_EXPECT_EQ(orcmap::RasterStrokeWidthPx(2.5f), 3);
  ORCMAP_EXPECT_EQ(orcmap::RasterStrokeWidthPx(3.0f), 3);
  ORCMAP_EXPECT_EQ(orcmap::RasterStrokeWidthPx(3.2f), 3);
}

void TestOnePxLinePreservesPrevious() {
  const orcmap::Color bg = orcmap::Color::Rgb(0, 0, 0);
  const orcmap::Color fg = orcmap::Color::Rgb(0, 255, 0);
  orcmap::host::FramebufferTarget fb(32, 32, bg);
  fb.DrawLine(0, 16, 31, 16, fg, 1.0f);
  for (int x = 0; x < 32; ++x) {
    ORCMAP_EXPECT_TRUE(fb.At(x, 16) == fg);
    ORCMAP_EXPECT_TRUE(fb.At(x, 15) == bg);
    ORCMAP_EXPECT_TRUE(fb.At(x, 17) == bg);
  }
}

void TestThreePxHorizontalThickness() {
  const orcmap::Color bg = orcmap::Color::Rgb(1, 1, 1);
  const orcmap::Color fg = orcmap::Color::Rgb(9, 9, 9);
  orcmap::host::FramebufferTarget fb(32, 32, bg);
  fb.DrawLine(4, 10, 20, 10, fg, 3.0f);
  ORCMAP_EXPECT_TRUE(fb.At(12, 9) == fg);
  ORCMAP_EXPECT_TRUE(fb.At(12, 10) == fg);
  ORCMAP_EXPECT_TRUE(fb.At(12, 11) == fg);
  ORCMAP_EXPECT_TRUE(fb.At(12, 8) == bg);
  ORCMAP_EXPECT_TRUE(fb.At(12, 12) == bg);
}

void TestTwoPxVertical() {
  const orcmap::Color bg = orcmap::Color::Rgb(0, 0, 0);
  const orcmap::Color fg = orcmap::Color::Rgb(255, 0, 0);
  orcmap::host::FramebufferTarget fb(16, 16, bg);
  fb.DrawLine(8, 2, 8, 12, fg, 2.0f);
  // Even width: FillRect(x - 1, y - 1, 2, 2) -> columns 7 and 8.
  ORCMAP_EXPECT_TRUE(fb.At(7, 6) == fg);
  ORCMAP_EXPECT_TRUE(fb.At(8, 6) == fg);
  ORCMAP_EXPECT_TRUE(fb.At(6, 6) == bg);
  ORCMAP_EXPECT_TRUE(fb.At(9, 6) == bg);
}

void TestDiagonalThick() {
  const orcmap::Color bg = orcmap::Color::Rgb(0, 0, 0);
  const orcmap::Color fg = orcmap::Color::Rgb(0, 0, 255);
  orcmap::host::FramebufferTarget fb(24, 24, bg);
  fb.DrawLine(2, 2, 18, 18, fg, 3.0f);
  ORCMAP_EXPECT_TRUE(fb.At(10, 10) == fg);
  ORCMAP_EXPECT_TRUE(fb.At(10, 9) == fg);
  ORCMAP_EXPECT_TRUE(fb.At(10, 11) == fg);
  ORCMAP_EXPECT_TRUE(CountColor(fb, fg) > 17);
}

void TestThickPartialOffscreen() {
  const orcmap::Color bg = orcmap::Color::Rgb(0, 0, 0);
  const orcmap::Color fg = orcmap::Color::Rgb(255, 255, 0);
  orcmap::host::FramebufferTarget fb(16, 16, bg);
  fb.DrawLine(-8, 8, 8, 8, fg, 3.0f);
  ORCMAP_EXPECT_TRUE(fb.At(0, 7) == fg);
  ORCMAP_EXPECT_TRUE(fb.At(0, 8) == fg);
  ORCMAP_EXPECT_TRUE(fb.At(0, 9) == fg);
  ORCMAP_EXPECT_TRUE(fb.At(8, 8) == fg);
}

void TestThickCenterlineJustOutside() {
  const orcmap::Color bg = orcmap::Color::Rgb(2, 2, 2);
  const orcmap::Color fg = orcmap::Color::Rgb(1, 2, 3);
  orcmap::host::FramebufferTarget fb(12, 12, bg);
  // Vertical centerline at x = -1; 3px brush must still paint x = 0.
  fb.DrawLine(-1, 1, -1, 10, fg, 3.0f);
  ORCMAP_EXPECT_TRUE(fb.At(0, 5) == fg);
  ORCMAP_EXPECT_TRUE(fb.At(1, 5) == bg);
}

void TestZeroWidthNoDraw() {
  const orcmap::Color bg = orcmap::Color::Rgb(4, 4, 4);
  orcmap::host::FramebufferTarget fb(8, 8, bg);
  fb.DrawLine(0, 4, 7, 4, orcmap::Color::Rgb(255, 0, 0), 0.0f);
  fb.DrawLine(0, 4, 7, 4, orcmap::Color::Rgb(255, 0, 0), -2.0f);
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      ORCMAP_EXPECT_TRUE(fb.At(x, y) == bg);
    }
  }
}

void TestLabelKindsProduceNoPixels() {
  orcmap::FeatureTile tile;
  auto add = [&](orcmap::FeatureKind kind) {
    orcmap::Feature f;
    f.kind = kind;
    f.kind_assigned = true;
    f.extent = 4096;
    f.geometry.type = orcmap::GeomType::kPoint;
    f.geometry.paths.push_back({{2048, 2048}});
    tile.features.push_back(f);
  };
  add(orcmap::FeatureKind::kLabelPrimary);
  add(orcmap::FeatureKind::kLabelSecondary);
  add(orcmap::FeatureKind::kLabelMuted);
  orcmap::host::FramebufferTarget fb(16, 16);
  const orcmap::Viewport vp = Z0(16);
  const auto& style = orcmap::styles::OrcSdrDark();
  ORCMAP_EXPECT_TRUE(orcmap::ClearMapBackground(vp, style, &fb));
  ORCMAP_EXPECT_TRUE(orcmap::RenderFeatureTile(tile, orcmap::TileId{0, 0, 0},
                                               vp, style, &fb));
  const orcmap::Color bg =
      orcmap::ResolveFeatureStyle(orcmap::FeatureKind::kBackground, 0, style)
          .color;
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      ORCMAP_EXPECT_TRUE(fb.At(x, y) == bg);
    }
  }
}

void TestClipRectRejectsInverted() {
  int x0 = 0, y0 = 0, x1 = 4, y1 = 4;
  ORCMAP_EXPECT_TRUE(!orcmap::ClipLineToPixelRect(&x0, &y0, &x1, &y1, 8, 0, 1, 8));
}

}  // namespace

void RunStrokeTests() {
  TestRasterWidthZeroNegative();
  TestRasterWidthOneAndBelow();
  TestRasterWidthFractional();
  TestOnePxLinePreservesPrevious();
  TestThreePxHorizontalThickness();
  TestTwoPxVertical();
  TestDiagonalThick();
  TestThickPartialOffscreen();
  TestThickCenterlineJustOutside();
  TestZeroWidthNoDraw();
  TestLabelKindsProduceNoPixels();
  TestClipRectRejectsInverted();
}
