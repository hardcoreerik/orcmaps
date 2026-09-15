#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "orcmap/pack.hpp"

namespace orcmap {

// Bounded JSON reader for pack manifests. Text in, PackManifest out -- no
// filesystem, no network, no dynamic schema. This is deliberately NOT a
// general-purpose JSON API: only the manifest shape documented in
// docs/PACK_MANIFEST_SCHEMA.md is accepted, and anything else is rejected
// rather than guessed at.
//
// The parser is hard-bounded so a malformed or hostile file on an SD card
// cannot exhaust memory or stack on an embedded device:
//   - input longer than kMaxPackManifestBytes is refused outright;
//   - nesting deeper than kMaxPackManifestDepth is refused;
//   - unknown keys are ignored (forward compatibility), but a key with the
//     wrong TYPE is an error, never a silent default.
//
// It does not set PackManifest::archive_path: that is a property of where
// the manifest was found, and belongs to the discovery layer.

inline constexpr size_t kMaxPackManifestBytes = 64 * 1024;
inline constexpr int kMaxPackManifestDepth = 12;

enum class PackJsonError {
  kNone,
  kTooLarge,      // exceeds kMaxPackManifestBytes
  kTooDeep,       // exceeds kMaxPackManifestDepth
  kMalformed,     // not valid JSON
  kNotAnObject,   // valid JSON but not a top-level object
  kWrongType,     // a known key had the wrong JSON type
  kMissingField,  // a required key was absent
  kOutOfRange,    // a numeric value does not fit its manifest field
};

const char* PackJsonErrorName(PackJsonError error);

// Parses `text` into `out`. On failure `out` is left unmodified and the
// reason is returned. Success here means "this is a well-formed manifest
// document"; it does NOT mean the manifest is valid or usable -- call
// ValidatePackManifest() for that.
PackJsonError ParsePackManifestJson(std::string_view text, PackManifest* out);

}  // namespace orcmap
