#include "orcmap/clip.hpp"

#include <cmath>

namespace orcmap {

namespace {

bool ClipT(double p, double q, double* t0, double* t1) {
  if (p == 0.0) return q >= 0.0;
  const double r = q / p;
  if (p < 0.0) {
    if (r > *t1) return false;
    if (r > *t0) *t0 = r;
  } else {
    if (r < *t0) return false;
    if (r < *t1) *t1 = r;
  }
  return true;
}

int ClampPixel(double v, int lo, int hi) {
  if (!std::isfinite(v)) return lo;
  if (v < static_cast<double>(lo)) return lo;
  if (v > static_cast<double>(hi)) return hi;
  return static_cast<int>(std::floor(v + 0.5));
}

}  // namespace

bool ClipLineToPixels(int* x0, int* y0, int* x1, int* y1, int width,
                      int height) {
  if (x0 == nullptr || y0 == nullptr || x1 == nullptr || y1 == nullptr) {
    return false;
  }
  if (width <= 0 || height <= 0) return false;

  const double xmin = 0.0;
  const double ymin = 0.0;
  const double xmax = static_cast<double>(width - 1);
  const double ymax = static_cast<double>(height - 1);
  const double ax = static_cast<double>(*x0);
  const double ay = static_cast<double>(*y0);
  const double bx = static_cast<double>(*x1);
  const double by = static_cast<double>(*y1);
  const double dx = bx - ax;
  const double dy = by - ay;
  double t0 = 0.0;
  double t1 = 1.0;
  // Liang-Barsky: clip parametric t against each edge of the pixel rect.
  if (!ClipT(-dx, ax - xmin, &t0, &t1)) return false;
  if (!ClipT(dx, xmax - ax, &t0, &t1)) return false;
  if (!ClipT(-dy, ay - ymin, &t0, &t1)) return false;
  if (!ClipT(dy, ymax - ay, &t0, &t1)) return false;

  const int max_x = width - 1;
  const int max_y = height - 1;
  *x0 = ClampPixel(ax + t0 * dx, 0, max_x);
  *y0 = ClampPixel(ay + t0 * dy, 0, max_y);
  *x1 = ClampPixel(ax + t1 * dx, 0, max_x);
  *y1 = ClampPixel(ay + t1 * dy, 0, max_y);
  return true;
}

}  // namespace orcmap
