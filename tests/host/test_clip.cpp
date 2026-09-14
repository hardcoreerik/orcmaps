#include "framebuffer_target.hpp"
#include "orcmap/clip.hpp"

#include <limits>

#include "test_util.hpp"

namespace {

bool AllPixels(const orcmap::host::FramebufferTarget& fb, orcmap::Color c) {
  for (int y = 0; y < fb.Height(); ++y) {
    for (int x = 0; x < fb.Width(); ++x) {
      if (fb.At(x, y) != c) return false;
    }
  }
  return true;
}

void TestClipFullyOutside() {
  int x0 = -1000000, y0 = -1000000, x1 = -900000, y1 = -900000;
  ORCMAP_EXPECT_TRUE(!orcmap::ClipLineToPixels(&x0, &y0, &x1, &y1, 32, 32));
}

void TestClipAcceptsOnscreenSegment() {
  int x0 = 2, y0 = 2, x1 = 10, y1 = 10;
  ORCMAP_EXPECT_TRUE(orcmap::ClipLineToPixels(&x0, &y0, &x1, &y1, 32, 32));
  ORCMAP_EXPECT_EQ(x0, 2);
  ORCMAP_EXPECT_EQ(y0, 2);
  ORCMAP_EXPECT_EQ(x1, 10);
  ORCMAP_EXPECT_EQ(y1, 10);
}

void TestClipEmptyRect() {
  int x0 = 0, y0 = 0, x1 = 1, y1 = 1;
  ORCMAP_EXPECT_TRUE(!orcmap::ClipLineToPixels(&x0, &y0, &x1, &y1, 0, 8));
  ORCMAP_EXPECT_TRUE(!orcmap::ClipLineToPixels(&x0, &y0, &x1, &y1, 8, 0));
}

void TestFullyOffscreenLineLeavesFramebufferUnchanged() {
  const orcmap::Color bg = orcmap::Color::Rgb(10, 20, 30);
  orcmap::host::FramebufferTarget fb(32, 32, bg);
  fb.DrawLine(-1000000, -1000000, -900000, -900000, orcmap::Color::Rgb(255, 0, 0));
  ORCMAP_EXPECT_TRUE(AllPixels(fb, bg));
}

void TestPartialOffscreenHorizontalLine() {
  const orcmap::Color bg = orcmap::Color::Rgb(0, 0, 0);
  const orcmap::Color fg = orcmap::Color::Rgb(0, 255, 0);
  orcmap::host::FramebufferTarget fb(32, 32, bg);
  fb.DrawLine(-1000000, 16, 1000000, 16, fg);
  ORCMAP_EXPECT_TRUE(fb.At(0, 16) == fg);
  ORCMAP_EXPECT_TRUE(fb.At(15, 16) == fg);
  ORCMAP_EXPECT_TRUE(fb.At(31, 16) == fg);
  ORCMAP_EXPECT_TRUE(fb.At(16, 0) == bg);
  ORCMAP_EXPECT_TRUE(fb.At(16, 31) == bg);
}

void TestHugeDiagonalClipsToVisibleSegment() {
  const orcmap::Color bg = orcmap::Color::Rgb(0, 0, 0);
  const orcmap::Color fg = orcmap::Color::Rgb(255, 255, 0);
  orcmap::host::FramebufferTarget fb(32, 32, bg);
  fb.DrawLine(-1000000, -1000000, 1000000, 1000000, fg);
  ORCMAP_EXPECT_TRUE(fb.At(0, 0) == fg);
  ORCMAP_EXPECT_TRUE(fb.At(16, 16) == fg);
  ORCMAP_EXPECT_TRUE(fb.At(31, 31) == fg);
  ORCMAP_EXPECT_TRUE(fb.At(31, 0) == bg);
  ORCMAP_EXPECT_TRUE(fb.At(0, 31) == bg);
}

void TestLargePolygonCoordinatesDoNotOverflow() {
  const orcmap::Color bg = orcmap::Color::Rgb(1, 1, 1);
  const orcmap::Color fg = orcmap::Color::Rgb(0, 0, 255);
  orcmap::host::FramebufferTarget fb(16, 16, bg);
  const int lo = std::numeric_limits<int>::min() / 2;
  const int hi = std::numeric_limits<int>::max() / 2;
  const int xy[] = {lo, lo, hi, lo, hi, hi, lo, hi};
  fb.FillPolygon(xy, 4, fg);
  ORCMAP_EXPECT_TRUE(fb.At(0, 0) == fg);
  ORCMAP_EXPECT_TRUE(fb.At(8, 8) == fg);
  ORCMAP_EXPECT_TRUE(fb.At(15, 15) == fg);
}

}  // namespace

void RunClipTests() {
  TestClipFullyOutside();
  TestClipAcceptsOnscreenSegment();
  TestClipEmptyRect();
  TestFullyOffscreenLineLeavesFramebufferUnchanged();
  TestPartialOffscreenHorizontalLine();
  TestHugeDiagonalClipsToVisibleSegment();
  TestLargePolygonCoordinatesDoNotOverflow();
}
