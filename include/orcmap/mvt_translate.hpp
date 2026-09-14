#pragma once

#include "orcmap/feature.hpp"
#include "orcmap/mvt.hpp"

namespace orcmap {

// Copies a decoded MVT tile into an OrcMaps-owned FeatureTile.
//
// This is the format translation boundary: MVT types stop here. The
// returned FeatureTile owns its strings, points, and properties and does
// not alias the input MvtTile -- the MvtTile may be destroyed afterwards.
//
// Does NOT assign FeatureKind (kind_assigned stays false). Semantic
// mapping is a separate, still-experimental step -- see
// include/orcmap/experimental/mvt_classify.hpp and docs/FORMAT_DECISION.md
// "Deferred: tile content schema".
//
// Returns false if `out` is null. An empty input tile is a successful
// translation to an empty FeatureTile.
bool TranslateMvtToFeatureTile(const MvtTile& in, FeatureTile* out);

}  // namespace orcmap
