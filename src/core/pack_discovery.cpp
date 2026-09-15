#include "orcmap/pack_discovery.hpp"

#include <algorithm>

namespace orcmap {
namespace {

constexpr char kManifestSuffix[] = ".manifest.json";
constexpr char kArchiveSuffix[] = ".pmtiles";

bool EndsWith(const std::string& text, const char* suffix) {
  const size_t n = std::string(suffix).size();
  return text.size() >= n && text.compare(text.size() - n, n, suffix) == 0;
}

std::string JoinPath(const std::string& dir, const std::string& name) {
  if (dir.empty()) return name;
  const char last = dir[dir.size() - 1];
  if (last == '/' || last == '\\') return dir + name;
  return dir + "/" + name;
}

}  // namespace

const char* PackRejectionName(PackRejection rejection) {
  switch (rejection) {
    case PackRejection::kUnreadable: return "unreadable";
    case PackRejection::kBadJson: return "bad-json";
    case PackRejection::kInvalidManifest: return "invalid-manifest";
    case PackRejection::kArchiveMissing: return "archive-missing";
    case PackRejection::kArchiveSizeMismatch: return "archive-size-mismatch";
    case PackRejection::kDuplicate: return "duplicate";
  }
  return "unknown";
}

std::string ArchivePathForManifest(const std::string& dir,
                                   const std::string& manifest_name) {
  if (!EndsWith(manifest_name, kManifestSuffix)) return std::string();
  const size_t stem = manifest_name.size() - std::string(kManifestSuffix).size();
  return JoinPath(dir, manifest_name.substr(0, stem) + kArchiveSuffix);
}

bool DiscoverPacks(PackFileSystem& filesystem, const std::string& dir,
                   PackCatalog* catalog, DiscoveryReport* report) {
  DiscoveryReport local;
  DiscoveryReport& out = report != nullptr ? *report : local;
  out = DiscoveryReport{};
  if (catalog == nullptr) return false;

  std::vector<std::string> names;
  if (!filesystem.ListManifestNames(dir, &names)) {
    out.directory_listed = false;
    return false;
  }
  out.directory_listed = true;

  // Deterministic order: the same card must produce the same catalogue on
  // every boot, whatever order the filesystem happens to hand back.
  std::sort(names.begin(), names.end());

  for (const std::string& name : names) {
    if (!EndsWith(name, kManifestSuffix)) continue;
    ++out.manifests_seen;

    RejectedPack rejected;
    rejected.manifest_name = name;

    std::string text;
    if (!filesystem.ReadTextFile(JoinPath(dir, name), kMaxPackManifestBytes,
                                 &text)) {
      rejected.rejection = PackRejection::kUnreadable;
      out.rejected.push_back(rejected);
      continue;
    }

    PackManifest manifest;
    const PackJsonError json_error = ParsePackManifestJson(text, &manifest);
    if (json_error != PackJsonError::kNone) {
      rejected.rejection = PackRejection::kBadJson;
      rejected.json_error = json_error;
      out.rejected.push_back(rejected);
      continue;
    }

    // The archive is named by the manifest's own filename, not by a path
    // stored inside it: a manifest on a card must not be able to point the
    // engine at an arbitrary file.
    const std::string archive = ArchivePathForManifest(dir, name);
    manifest.archive_path = archive;

    // Validate BEFORE touching the archive, so an incompatible or
    // unsupported manifest is reported as such rather than as a missing
    // file. Unknown schema profiles are refused here, never guessed.
    const PackValidationError validation = ValidatePackManifest(manifest);
    if (validation != PackValidationError::kNone) {
      rejected.rejection = PackRejection::kInvalidManifest;
      rejected.validation_error = validation;
      out.rejected.push_back(rejected);
      continue;
    }

    uint64_t actual_size = 0;
    if (!filesystem.FileSize(archive, &actual_size)) {
      rejected.rejection = PackRejection::kArchiveMissing;
      rejected.detail = archive;
      out.rejected.push_back(rejected);
      continue;
    }
    if (actual_size != manifest.size_bytes) {
      // Cheap integrity signal that costs no I/O. A full SHA-256 of a
      // multi-megabyte archive is a separate, measured step; a wrong size
      // is already proof the pair does not belong together.
      rejected.rejection = PackRejection::kArchiveSizeMismatch;
      rejected.detail = archive;
      out.rejected.push_back(rejected);
      continue;
    }

    if (!catalog->Add(manifest)) {
      // PackCatalog::Add refuses duplicates and anything it considers
      // invalid; validation already passed, so this is an identity clash.
      rejected.rejection = PackRejection::kDuplicate;
      out.rejected.push_back(rejected);
      continue;
    }
    ++out.packs_added;
  }

  return out.rejected.empty();
}

}  // namespace orcmap
