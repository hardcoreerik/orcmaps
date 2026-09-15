#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "orcmap/pack.hpp"
#include "orcmap/pack_json.hpp"

namespace orcmap {

// Runtime discovery of packs a user copied onto local storage.
//
// Completely offline: there is no network catalog, no remote index, and no
// fallback of any kind. Discovery reads one directory, validates what it
// finds, and reports everything it rejected.
//
// The filesystem sits behind PackFileSystem so core stays free of any
// filesystem dependency (the same reason ByteSource exists). The board or
// application supplies both the implementation and the directory path --
// OrcMaps core does not know what an SD card is, and does not know the
// string "/sd/orcmaps".
//
// Directory contract, deliberately simple and flat:
//   <dir>/<name>.manifest.json   pack metadata
//   <dir>/<name>.pmtiles         the archive it describes
// The archive is located by replacing the manifest suffix, so a manifest
// names its own archive by position rather than by a path inside the file.
// Subdirectories are NOT traversed: discovery lists exactly one directory.

class PackFileSystem {
 public:
  virtual ~PackFileSystem() = default;

  // Names (not full paths) of entries in `dir` ending in ".manifest.json".
  // Must not recurse. Returns false if the directory cannot be listed at
  // all, which is distinct from an empty directory.
  virtual bool ListManifestNames(const std::string& dir,
                                 std::vector<std::string>* out) = 0;

  // Reads at most `max_bytes` of a text file. Returns false if the file
  // cannot be read or is larger than `max_bytes`.
  virtual bool ReadTextFile(const std::string& path, size_t max_bytes,
                            std::string* out) = 0;

  // True if the path exists as a regular file; writes its size if `size`
  // is non-null.
  virtual bool FileSize(const std::string& path, uint64_t* size) = 0;
};

enum class PackRejection {
  kUnreadable,       // manifest file could not be read (or was too large)
  kBadJson,          // manifest is not a well-formed manifest document
  kInvalidManifest,  // well-formed but ValidatePackManifest() refused it
  kArchiveMissing,   // manifest is fine but its .pmtiles is absent
  kArchiveSizeMismatch,  // archive present but not the size the manifest claims
  kDuplicate,        // another manifest already provided this pack identity
};

const char* PackRejectionName(PackRejection rejection);

struct RejectedPack {
  std::string manifest_name;
  PackRejection rejection = PackRejection::kUnreadable;
  // Only meaningful when rejection == kBadJson.
  PackJsonError json_error = PackJsonError::kNone;
  // Only meaningful when rejection == kInvalidManifest.
  PackValidationError validation_error = PackValidationError::kNone;
  std::string detail;  // e.g. the archive path that was missing
};

struct DiscoveryReport {
  bool directory_listed = false;  // false means the directory itself failed
  size_t manifests_seen = 0;
  size_t packs_added = 0;
  std::vector<RejectedPack> rejected;
};

// Scans `dir`, adds every valid pack whose archive is present to `catalog`,
// and records every rejection in `report`. Returns true only when the
// directory was listable AND nothing was rejected -- a caller that merely
// wants "did we get any maps" should check `report->packs_added`.
//
// Rejections are never fatal to the scan: one malformed manifest on a card
// must not hide the packs beside it.
bool DiscoverPacks(PackFileSystem& filesystem, const std::string& dir,
                   PackCatalog* catalog, DiscoveryReport* report);

// The archive path a manifest file implies: "x.manifest.json" -> "x.pmtiles"
// in the same directory. Returns an empty string if `manifest_name` does not
// end in ".manifest.json". Exposed for tests and for callers that want to
// report the expected path.
std::string ArchivePathForManifest(const std::string& dir,
                                   const std::string& manifest_name);

}  // namespace orcmap
