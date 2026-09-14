#pragma once

namespace orcmap {

// Liang-Barsky clip of a line to the inclusive pixel rectangle
// [0, width-1] x [0, height-1]. Returns false if the segment is fully
// outside or the rectangle is empty. On true, endpoints lie in-range.
//
// Implemented here from the parametric line / edge-constraint formulation
// (not copied from another codebase). Doubles are used for t so huge
// offscreen endpoints cannot overflow an integer intercept multiply.

bool ClipLineToPixels(int* x0, int* y0, int* x1, int* y1, int width,
                      int height);

}  // namespace orcmap
