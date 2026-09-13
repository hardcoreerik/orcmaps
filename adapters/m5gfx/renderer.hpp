#pragma once

#include <utility>

#include <M5GFX.h>

#include "color.hpp"
#include "orcmap/mvt.hpp"
#include "orcmap/style.hpp"

namespace orcmap::m5gfx_adapter {

// Minimal M5GFX/LovyanGFX draw backend. Takes `lgfx::v1::LovyanGFX&`
// (the portable LovyanGFX interface), matching ORCMAP1's existing
// portable-overload pattern (docs/ORCMAP1_AUDIT.md SS4) rather than
// coupling to `M5.Display`/`M5GFX` concretely -- any LovyanGFX-compatible
// display (real hardware, a sprite, a future host-side test renderer that
// implements the same interface) can use these functions.
//
// Deliberately minimal: this draws geometry a caller has already decided
// to draw, with a paint the caller has already resolved
// (orcmap::ResolveFeatureStyle). It has no opinion about what a decoded
// MvtFeature's layer/attributes MEAN (i.e. which orcmap::FeatureKind a
// "road" feature is) -- that mapping is still blocked on the tile content
// schema decision (docs/FORMAT_DECISION.md "Deferred"). This file proves
// the M5GFX/LovyanGFX draw path itself works end-to-end from decoded MVT
// geometry through the style system's paint to real pixels; it does not
// attempt the schema decision it deliberately isn't this file's job to
// make.

// Maps a decoded tile's local coordinate space (0..extent, per the MVT
// spec) linearly onto a screen-space rectangle. This is NOT the final
// pan/zoom/tile-compositing math -- that depends on the Viewport design,
// still unbuilt (ROADMAP.md Phase 2/3) -- it is the minimum transform
// needed to draw one tile's geometry into one screen-space rectangle for
// a first visible result.
struct TileViewport {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  uint32_t extent = 4096;
};

inline std::pair<int, int> ProjectTileLocal(const MvtPoint& p, const TileViewport& viewport) {
  const int64_t extent = viewport.extent == 0 ? 1 : viewport.extent;
  const int px = viewport.x + static_cast<int>((static_cast<int64_t>(p.x) * viewport.width) / extent);
  const int py = viewport.y + static_cast<int>((static_cast<int64_t>(p.y) * viewport.height) / extent);
  return {px, py};
}

// Draws one geometry ring as connected line segments. `closed` joins the
// last point back to the first (used for polygon rings; MVT's own
// ClosePath command carries no coordinates -- see
// src/tiles/mvt_decoder.cpp -- so the caller decides ring closure based
// on the feature's geom_type).
inline void DrawRing(lgfx::v1::LovyanGFX& display, const MvtRing& ring,
                     const TileViewport& viewport, uint16_t color565, bool closed) {
  if (ring.size() < 2) return;
  for (size_t i = 0; i + 1 < ring.size(); ++i) {
    const auto [x1, y1] = ProjectTileLocal(ring[i], viewport);
    const auto [x2, y2] = ProjectTileLocal(ring[i + 1], viewport);
    display.drawLine(x1, y1, x2, y2, color565);
  }
  if (closed && ring.size() > 2) {
    const auto [x1, y1] = ProjectTileLocal(ring.back(), viewport);
    const auto [x2, y2] = ProjectTileLocal(ring.front(), viewport);
    display.drawLine(x1, y1, x2, y2, color565);
  }
}

// Draws one decoded feature using an already-resolved paint
// (orcmap::ResolveFeatureStyle). No-op if `paint.visible` is false --
// callers are not expected to filter invisible features before calling
// this.
inline void DrawFeature(lgfx::v1::LovyanGFX& display, const MvtFeature& feature,
                        const TileViewport& viewport, const MapPaint& paint) {
  if (!paint.visible) return;
  const uint16_t color565 = ToRgb565(paint.color);
  const bool closed = feature.geom_type == MvtGeomType::kPolygon;
  for (const auto& ring : feature.geometry) {
    DrawRing(display, ring, viewport, color565, closed);
  }
}

}  // namespace orcmap::m5gfx_adapter
