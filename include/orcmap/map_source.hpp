#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "orcmap/attribution.hpp"

namespace orcmap {

// Describes one loaded/discoverable map source -- e.g. one installed
// .pmtiles pack, or one logical layer within a pack composition (see
// docs/DATA_AND_LICENSING.md "Keep license classes separable" for why a
// single view may compose several MapSourceInfo instances rather than one
// monolithic archive).
//
// This is a plain descriptive struct, not a loader -- opening the actual
// archive is orcmap::PmTilesReader's job (include/orcmap/pmtiles.hpp).
// MapSourceInfo is what a future MapEngine/pack-discovery API
// (orcmap::discoverPacks(), not yet implemented -- see ROADMAP.md) would
// return to describe what's installed, without the caller needing to know
// anything about PMTiles internals.
struct MapSourceInfo {
  std::string id;            // Matches a data/sources/<id>.json provenance record's `id`.
  std::string pack_id;       // Which installed pack (docs/PACK_MANIFEST_SCHEMA.md) this came from.
  std::string display_name;

  double min_lon_deg = 0.0;
  double min_lat_deg = 0.0;
  double max_lon_deg = 0.0;
  double max_lat_deg = 0.0;
  uint8_t min_zoom = 0;
  uint8_t max_zoom = 0;

  AttributionInfo attribution;
};

// Collects the attribution text for every active source that requires it,
// deduplicated by text -- the shape `map.activeSources()` +
// per-source `requiresAttribution()` iteration (docs/DATA_AND_LICENSING.md
// "Runtime attribution") reduces to for a caller that just wants "what do
// I need to display right now."
inline std::vector<AttributionInfo> CollectRequiredAttribution(
    const std::vector<MapSourceInfo>& sources) {
  std::vector<AttributionInfo> required;
  for (const auto& source : sources) {
    if (!source.attribution.required) continue;
    bool duplicate = false;
    for (const auto& existing : required) {
      if (existing.text == source.attribution.text) {
        duplicate = true;
        break;
      }
    }
    if (!duplicate) required.push_back(source.attribution);
  }
  return required;
}

}  // namespace orcmap
