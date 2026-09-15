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

// Single-feature translation, for streaming decode (MvtDecodeOptions::
// feature_sink). Writes into `out`, reusing its existing capacity instead of
// returning a fresh Feature, so a tile's features can be translated one at a
// time without the allocation churn -- or the peak memory -- of building a
// whole FeatureTile. `layer` supplies extent and layer name; its own
// `features` vector is not read.
bool TranslateMvtFeature(const MvtLayer& layer, const MvtFeature& in,
                         Feature* out);

}  // namespace orcmap
