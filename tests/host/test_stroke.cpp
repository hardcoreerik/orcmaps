#include "framebuffer_target.hpp"
#include "orcmap/clip.hpp"

#include <limits>
#include <vector>
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

// The run-coalescing rasterizer must paint EXACTLY what a naive
// one-rect-per-pixel brush paints. This compares the two over a spread of
// angles, widths, and partially-offscreen cases rather than trusting the
// geometric argument, and separately checks that the optimization actually
// reduces the number of fill_rect calls.
constexpr int kStrokeW = 64;
constexpr int kStrokeH = 48;

void PaintRect(std::vector<uint8_t>* buf, int x, int y, int w, int h) {
  for (int yy = y; yy < y + h; ++yy) {
    if (yy < 0 || yy >= kStrokeH) continue;
    for (int xx = x; xx < x + w; ++xx) {
      if (xx < 0 || xx >= kStrokeW) continue;
      (*buf)[yy * kStrokeW + xx] = 1;
    }
  }
}

void TestRunCoalescingIsPixelIdenticalToPerPixelBrush() {
  constexpr int kW = kStrokeW;
  constexpr int kH = kStrokeH;
  const int lines[][4] = {
      {2, 2, 60, 2},     {60, 2, 2, 2},     {2, 2, 2, 44},
      {2, 44, 2, 2},     {0, 0, 63, 47},    {63, 47, 0, 0},
      {5, 40, 58, 6},    {1, 1, 62, 20},    {1, 20, 62, 1},
      {-20, 10, 80, 12}, {30, -15, 32, 70}, {-5, -5, 70, 60},
      {10, 10, 10, 10},  {0, 47, 63, 0},
  };
  const float widths[] = {1.0f, 1.8f, 2.0f, 2.5f, 3.0f, 5.0f};

  for (const float width : widths) {
    for (const auto& line : lines) {
      std::vector<uint8_t> optimized(kW * kH, 0);
      std::vector<uint8_t> reference(kW * kH, 0);
      int optimized_calls = 0;
      int reference_calls = 0;

      orcmap::RasterizeCenteredStroke(
          line[0], line[1], line[2], line[3], width, kW, kH,
          [&](int x, int y, int w, int h) {
            ++optimized_calls;
            PaintRect(&optimized, x, y, w, h);
          });

      // Reference: the previous behaviour -- one w x w rect centred on every
      // Bresenham pixel, with the same clip rect the real one uses.
      const int rw = orcmap::RasterStrokeWidthPx(width);
      if (rw > 0) {
        const int r = rw / 2;
        int x0 = line[0], y0 = line[1], x1 = line[2], y1 = line[3];
        if (orcmap::ClipLineToPixelRect(&x0, &y0, &x1, &y1, -r, -r,
                                        kW - 1 + r, kH - 1 + r)) {
          const auto iabs = [](int v) { return v < 0 ? -v : v; };
          int dx = iabs(x1 - x0);
          int sx = x0 < x1 ? 1 : -1;
          int dy = -iabs(y1 - y0);
          int sy = y0 < y1 ? 1 : -1;
          int err = dx + dy;
          for (;;) {
            ++reference_calls;
            PaintRect(&reference, x0 - r, y0 - r, rw, rw);
            if (x0 == x1 && y0 == y1) break;
            const int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
          }
        }
      }

      ORCMAP_EXPECT_TRUE(optimized == reference);
      // Never worse than the per-pixel version.
      ORCMAP_EXPECT_TRUE(optimized_calls <= reference_calls ||
                         reference_calls == 0);
    }
  }
}

void TestRunCoalescingCollapsesAxisAlignedRuns() {
  // A long horizontal 3 px stroke is the common case (roads), and must
  // collapse to a single rectangle instead of one per pixel.
  int calls = 0;
  orcmap::RasterizeCenteredStroke(4, 20, 120, 20, 3.0f, 200, 40,
                                  [&](int, int, int, int) { ++calls; });
  ORCMAP_EXPECT_EQ(calls, 1);

  calls = 0;
  orcmap::RasterizeCenteredStroke(20, 4, 20, 120, 3.0f, 40, 200,
                                  [&](int, int, int, int) { ++calls; });
  ORCMAP_EXPECT_EQ(calls, 1);

  // A pure diagonal cannot collapse: every step moves in both axes, so it
  // still costs one rect per pixel. Recording this keeps the optimization
  // honest about what it does not help.
  calls = 0;
  orcmap::RasterizeCenteredStroke(0, 0, 63, 63, 3.0f, 64, 64,
                                  [&](int, int, int, int) { ++calls; });
  ORCMAP_EXPECT_EQ(calls, 64);
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
  TestRunCoalescingIsPixelIdenticalToPerPixelBrush();
  TestRunCoalescingCollapsesAxisAlignedRuns();
}
