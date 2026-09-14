#pragma once

#include <cmath>
#include <cstddef>

#include "orcmap/clip.hpp"

namespace orcmap {

// Shared map-stroke contract for every RenderTarget.
//
// RasterStrokeWidthPx:
//   width_px <= 0 or non-finite -> 0 (no draw)
//   0 < width_px <= 1           -> 1 (current one-pixel line)
//   width_px > 1                -> nearest integer, halves round away
//                                  from zero via floor(w+0.5), minimum 2
//
// RasterizeCenteredStroke paints an axis-aligned square brush of that
// integer width, centered on each Bresenham centerline pixel
// (top-left bias for even widths: FillRect(x - w/2, y - w/2, w, w)).
// No round caps, no miter/bevel joins, no dashes.
//
// Thick-line clipping expands the Liang-Barsky rectangle by w/2 so a
// centerline just outside the framebuffer can still stamp visible pixels.

inline constexpr int kMaxRasterStrokePx = 64;

inline int RasterStrokeWidthPx(float width_px) {
  if (!std::isfinite(width_px) || width_px <= 0.0f) return 0;
  if (width_px <= 1.0f) return 1;
  const double rounded = std::floor(static_cast<double>(width_px) + 0.5);
  int w = static_cast<int>(rounded);
  if (w < 2) w = 2;
  if (w > kMaxRasterStrokePx) w = kMaxRasterStrokePx;
  return w;
}

template <typename FillRect>
void RasterizeCenteredStroke(int x0, int y0, int x1, int y1, float width_px,
                             int fb_w, int fb_h, FillRect&& fill_rect) {
  const int w = RasterStrokeWidthPx(width_px);
  if (w <= 0 || fb_w <= 0 || fb_h <= 0) return;
  const int r = w / 2;
  const int xmin = -r;
  const int ymin = -r;
  const int xmax = fb_w - 1 + r;
  const int ymax = fb_h - 1 + r;
  if (!ClipLineToPixelRect(&x0, &y0, &x1, &y1, xmin, ymin, xmax, ymax)) {
    return;
  }

  auto iabs = [](int v) { return v < 0 ? -v : v; };
  int dx = iabs(x1 - x0);
  int sx = x0 < x1 ? 1 : -1;
  int dy = -iabs(y1 - y0);
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    fill_rect(x0 - r, y0 - r, w, w);
    if (x0 == x1 && y0 == y1) break;
    const int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

}  // namespace orcmap
