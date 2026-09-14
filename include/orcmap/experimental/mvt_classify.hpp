#pragma once

#include "orcmap/feature.hpp"

namespace orcmap {
namespace experimental {

// EXPERIMENTAL / NOT STABLE PUBLIC API.
//
// Temporary layer-name + property heuristic so tests (and a later render
// proof) can attach FeatureKind without pretending the tile-content schema
// is decided. The real mapping waits for a measured Lane County /
// OpenMapTiles (or narrower) tile -- see docs/FORMAT_DECISION.md
// "Deferred: tile content schema".
//
// Replace this file without changing Feature, Geometry, FeatureTile, the
// style system, Viewport, or graphics integrations.
//
// Unrecognized features are left with kind_assigned == false. This is not
// a production classifier and must not be treated as the OrcMaps schema.

// Returns true and writes `kind` if the heuristic recognizes `feature`.
bool TryClassifyFeature(const Feature& feature, FeatureKind* kind);

// Best-effort: sets kind/kind_assigned only on features this heuristic
// recognizes. Returns false if `tile` is null; empty tiles succeed.
bool AssignFeatureKinds(FeatureTile* tile);

}  // namespace experimental
}  // namespace orcmap
