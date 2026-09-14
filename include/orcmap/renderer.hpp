#pragma once

#include "orcmap/feature.hpp"
#include "orcmap/geo.hpp"
#include "orcmap/render_target.hpp"
#include "orcmap/style.hpp"
#include "orcmap/viewport.hpp"

namespace orcmap {

// Walks an OrcMaps FeatureTile, resolves paint via ResolveFeatureStyle,
// and issues primitives to `target`. No MVT types, no graphics-framework
// types.
//
// Features with kind_assigned == false are skipped (not guessed).
// Polygon fill uses the first path only as a simple outer ring; additional
// paths (holes) are not subtracted -- see ARCHITECTURE.md.
// Line width_px is not rasterized yet (1px strokes).
//
// `source_tile` is the z/x/y the FeatureTile was decoded from.
// `source_tile.z` should match `viewport.zoom` (PARTIAL: no overzoom).
// `target` null is a no-op.

void RenderFeatureTile(const FeatureTile& features, TileId source_tile,
                       const Viewport& viewport, const MapStyle& style,
                       RenderTarget* target);

}  // namespace orcmap
