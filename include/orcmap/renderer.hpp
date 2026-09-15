#pragma once

#include "orcmap/feature.hpp"
#include "orcmap/geo.hpp"
#include "orcmap/render_target.hpp"
#include "orcmap/style.hpp"
#include "orcmap/viewport.hpp"

namespace orcmap {

// Fills the whole target with the style background, once per map frame.
// Must be called before the per-tile RenderFeatureTile draws; tiles do
// not clear the target. Color comes from ResolveFeatureStyle(kBackground).
// Returns false if `target` is null or viewport.zoom is invalid.

bool ClearMapBackground(const Viewport& viewport, const MapStyle& style,
                        RenderTarget* target);

// Walks an OrcMaps FeatureTile, resolves paint via ResolveFeatureStyle,
// and issues primitives to `target`. No MVT types, no graphics-framework
// types. Does not clear the target.
//
// Returns false (and draws nothing) if:
// - target is null
// - viewport.zoom is not in 0..31
// - source_tile.z != viewport.zoom (overzoom is not implemented)
//
// Features with kind_assigned == false are skipped (not guessed).
// Label kinds (kLabelPrimary/Secondary/Muted) are skipped until text
// rendering exists -- they are not drawn as placeholder point pixels.
// Polygon fill uses the first path only as a simple outer ring; additional
// paths (holes) are not subtracted -- see ARCHITECTURE.md.
// Line strokes use MapPaint::width_px via RenderTarget::DrawLine.

bool RenderFeatureTile(const FeatureTile& features, TileId source_tile,
                       const Viewport& viewport, const MapStyle& style,
                       RenderTarget* target);

// As RenderFeatureTile, but draws the tile at a specific world copy. Pair it
// with EnumerateVisibleTilePlacements so a world narrower than the viewport
// extends around itself instead of leaving empty margin. Decode each distinct
// TileId once and call this per placement.
bool RenderFeatureTileAt(const FeatureTile& features,
                         const TilePlacement& placement,
                         const Viewport& viewport, const MapStyle& style,
                         RenderTarget* target);

}  // namespace orcmap
