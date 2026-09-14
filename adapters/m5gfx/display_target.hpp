#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

#include <M5GFX.h>

#include "color.hpp"
#include "orcmap/clip.hpp"
#include "orcmap/render_target.hpp"

namespace orcmap {
namespace m5gfx_adapter {

// M5GFX/LovyanGFX RenderTarget. Map semantics (style, viewport, FeatureKind,
// MVT) stay in OrcMaps core. This class only converts Color to RGB565 and
// issues LovyanGFX primitives. Takes lgfx::v1::LovyanGFX& so it works with
// a hardware panel, an M5Canvas/LGFX_Sprite, or any LovyanGFX surface.
//
// Line visibility uses OrcMaps ClipLineToPixels, not LovyanGFX's clip, so
// host and M5GFX agree. FillPolygon is even-odd scanline of the given
// vertices (the renderer already passes first path only). 1px strokes;
// width_px is not this adapter's job.

class DisplayTarget : public RenderTarget {
 public:
  explicit DisplayTarget(lgfx::v1::LovyanGFX& display) : display_(display) {}

  int Width() const override { return display_.width(); }
  int Height() const override { return display_.height(); }

  void FillRect(int x, int y, int w, int h, Color color) override {
    if (w <= 0 || h <= 0) return;
    const int width = Width();
    const int height = Height();
    int64_t x0 = x < 0 ? 0 : static_cast<int64_t>(x);
    int64_t y0 = y < 0 ? 0 : static_cast<int64_t>(y);
    int64_t x1 = static_cast<int64_t>(x) + static_cast<int64_t>(w);
    int64_t y1 = static_cast<int64_t>(y) + static_cast<int64_t>(h);
    if (x1 > width) x1 = width;
    if (y1 > height) y1 = height;
    if (x0 >= x1 || y0 >= y1) return;
    display_.fillRect(static_cast<int>(x0), static_cast<int>(y0),
                      static_cast<int>(x1 - x0), static_cast<int>(y1 - y0),
                      ToRgb565(color));
  }

  void DrawPoint(int x, int y, Color color) override {
    display_.drawPixel(x, y, ToRgb565(color));
  }

  void DrawLine(int x0, int y0, int x1, int y1, Color color) override {
    if (!ClipLineToPixels(&x0, &y0, &x1, &y1, Width(), Height())) return;
    display_.drawLine(x0, y0, x1, y1, ToRgb565(color));
  }

  void FillPolygon(const int* xy_pairs, size_t n_points, Color color) override {
    if (xy_pairs == nullptr || n_points < 3) return;
    const int width = Width();
    const int height = Height();
    if (width <= 0 || height <= 0) return;
    int min_y = xy_pairs[1];
    int max_y = xy_pairs[1];
    int min_x = xy_pairs[0];
    int max_x = xy_pairs[0];
    for (size_t i = 0; i < n_points; ++i) {
      const int px = xy_pairs[i * 2];
      const int py = xy_pairs[i * 2 + 1];
      if (py < min_y) min_y = py;
      if (py > max_y) max_y = py;
      if (px < min_x) min_x = px;
      if (px > max_x) max_x = px;
    }
    if (max_y < 0 || min_y >= height || max_x < 0 || min_x >= width) return;
    if (min_y < 0) min_y = 0;
    if (max_y >= height) max_y = height - 1;
    const uint16_t rgb = ToRgb565(color);
    std::vector<int> crossings;
    crossings.reserve(n_points);
    display_.startWrite();
    for (int y = min_y; y <= max_y; ++y) {
      crossings.clear();
      for (size_t i = 0; i < n_points; ++i) {
        const size_t j = (i + 1) % n_points;
        const int y0 = xy_pairs[i * 2 + 1];
        const int y1 = xy_pairs[j * 2 + 1];
        const int x0 = xy_pairs[i * 2];
        const int x1 = xy_pairs[j * 2];
        if (y0 == y1) continue;
        const int lo = y0 < y1 ? y0 : y1;
        const int hi = y0 < y1 ? y1 : y0;
        if (y < lo || y >= hi) continue;
        const int64_t num =
            (static_cast<int64_t>(y) - static_cast<int64_t>(y0)) *
            (static_cast<int64_t>(x1) - static_cast<int64_t>(x0));
        const int64_t den =
            static_cast<int64_t>(y1) - static_cast<int64_t>(y0);
        const int64_t x = static_cast<int64_t>(x0) + num / den;
        if (x > static_cast<int64_t>(std::numeric_limits<int>::max())) {
          crossings.push_back(std::numeric_limits<int>::max());
        } else if (x < static_cast<int64_t>(std::numeric_limits<int>::min())) {
          crossings.push_back(std::numeric_limits<int>::min());
        } else {
          crossings.push_back(static_cast<int>(x));
        }
      }
      std::sort(crossings.begin(), crossings.end());
      for (size_t k = 0; k + 1 < crossings.size(); k += 2) {
        int xa = crossings[k];
        int xb = crossings[k + 1];
        if (xb < xa) {
          const int t = xa;
          xa = xb;
          xb = t;
        }
        if (xa < 0) xa = 0;
        if (xb >= width) xb = width - 1;
        if (xb < xa) continue;
        display_.drawFastHLine(xa, y, xb - xa + 1, rgb);
      }
    }
    display_.endWrite();
  }

 private:
  lgfx::v1::LovyanGFX& display_;
};

}  // namespace m5gfx_adapter
}  // namespace orcmap
