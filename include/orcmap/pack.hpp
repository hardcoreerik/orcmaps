#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "orcmap/attribution.hpp"

namespace orcmap {

struct GeoBounds {
  double min_lon_deg = 0.0;
  double min_lat_deg = 0.0;
  double max_lon_deg = 0.0;
  double max_lat_deg = 0.0;
};

struct PackManifest {
  std::string pack_id;
  std::string pack_version;
  std::string display_name;
  std::string region_id;
  std::string region_name;
  GeoBounds bounds;
  uint8_t min_zoom = 0;
  uint8_t max_zoom = 0;
  std::string content_profile;
  uint8_t pmtiles_version = 0;
  std::string schema_version;
  std::string source_snapshot;
  std::string builder;
  std::string builder_version;
  std::string builder_commit;
  std::string build_date;
  std::vector<std::string> provenance_ids;
  std::string pack_class;
  std::vector<AttributionInfo> attribution;
  uint64_t size_bytes = 0;
  std::string output_sha256;
  int priority = 0;
  std::string archive_path;
};

enum class PackValidationError {
  kNone,
  kRequiredField,
  kBounds,
  kZoomRange,
  kCompatibility,
  kProvenance,
  kAttribution,
  kArchive,
  kIdentity,
};

std::string MakePackId(const PackManifest& manifest);
PackValidationError ValidatePackManifest(const PackManifest& manifest);

class PackCatalog {
 public:
  bool Add(PackManifest manifest);
  const std::vector<PackManifest>& Packs() const { return packs_; }

 private:
  std::vector<PackManifest> packs_;
};

// Returns exactly one eligible local basemap, or nullptr when installed data
// cannot satisfy the requested bounds and zoom. This function has no network
// fallback by design.
const PackManifest* ResolvePack(const PackCatalog& catalog,
                                const GeoBounds& requested, uint8_t zoom);

}  // namespace orcmap
