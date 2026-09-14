#pragma once

#include <cstddef>

#include "orcmap/color.hpp"

namespace orcmap {

// Graphics-library-independent draw surface. Understands pixels and
// primitives, not FeatureKind, MVT, PMTiles, or application overlays.
// Immediate: the renderer issues a primitive and the target draws it.
// There is no mandatory command buffer.
//
// Color is orcmap::Color (RGBA8). Conversion to a display's native pixel
// format is the target's job.

class RenderTarget {
 public:
  virtual ~RenderTarget() = default;

  virtual int Width() const = 0;
  virtual int Height() const = 0;

  virtual void FillRect(int x, int y, int w, int h, Color color) = 0;
  virtual void DrawPoint(int x, int y, Color color) = 0;

  // Centered stroke. `width_px` comes from MapPaint (the style), never
  // from FeatureKind inside an adapter. See include/orcmap/stroke.hpp.
  virtual void DrawLine(int x0, int y0, int x1, int y1, Color color,
                        float width_px) = 0;

  // Simple filled polygon, `n_points` vertices as [x0,y0,x1,y1,...].
  // No holes. n_points < 3 is a no-op. The target clips to its bounds.
  virtual void FillPolygon(const int* xy_pairs, size_t n_points, Color color) = 0;
};

}  // namespace orcmap
