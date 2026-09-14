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

bool ClipLineToPixelRect(int* x0, int* y0, int* x1, int* y1, int xmin,
                         int ymin, int xmax, int ymax) {
  if (x0 == nullptr || y0 == nullptr || x1 == nullptr || y1 == nullptr) {
    return false;
  }
  if (xmin > xmax || ymin > ymax) return false;

  const double xmin_d = static_cast<double>(xmin);
  const double ymin_d = static_cast<double>(ymin);
  const double xmax_d = static_cast<double>(xmax);
  const double ymax_d = static_cast<double>(ymax);
  const double ax = static_cast<double>(*x0);
  const double ay = static_cast<double>(*y0);
  const double bx = static_cast<double>(*x1);
  const double by = static_cast<double>(*y1);
  const double dx = bx - ax;
  const double dy = by - ay;
  double t0 = 0.0;
  double t1 = 1.0;
  if (!ClipT(-dx, ax - xmin_d, &t0, &t1)) return false;
  if (!ClipT(dx, xmax_d - ax, &t0, &t1)) return false;
  if (!ClipT(-dy, ay - ymin_d, &t0, &t1)) return false;
  if (!ClipT(dy, ymax_d - ay, &t0, &t1)) return false;

  *x0 = ClampPixel(ax + t0 * dx, xmin, xmax);
  *y0 = ClampPixel(ay + t0 * dy, ymin, ymax);
  *x1 = ClampPixel(ax + t1 * dx, xmin, xmax);
  *y1 = ClampPixel(ay + t1 * dy, ymin, ymax);
  return true;
}

bool ClipLineToPixels(int* x0, int* y0, int* x1, int* y1, int width,
                      int height) {
  if (width <= 0 || height <= 0) return false;
  return ClipLineToPixelRect(x0, y0, x1, y1, 0, 0, width - 1, height - 1);
}

}  // namespace orcmap
