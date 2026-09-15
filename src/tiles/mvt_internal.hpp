#pragma once

// INTERNAL to src/tiles: the pieces of the MVT decoder shared between the
// materialising decoder (mvt_decoder.cpp) and the end-to-end streaming
// parser (mvt_stream.cpp). Not a public header, not installed, and not part
// of the OrcMaps API -- it exists only so the two parsers cannot drift into
// separate implementations of the same wire format.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "orcmap/feature.hpp"
#include "orcmap/mvt.hpp"

namespace orcmap {
namespace internal {

// Decodes one MVT Value message.
bool DecodeValue(const uint8_t* data, size_t length, MvtValue* out);

// Decodes one MVT Feature message, resolving its tag indices against the
// owning layer's key/value tables.
bool DecodeFeature(const uint8_t* data, size_t length,
                   const std::vector<std::string>& layer_keys,
                   const std::vector<MvtValue>& layer_values, MvtFeature* out);

// Reusable buffers for the packed tag/geometry command arrays, so a caller
// decoding many features in a row does not re-allocate them per feature.
struct FeatureParseScratch {
  std::vector<uint32_t> tags;
  std::vector<uint32_t> geometry;
};

// Decodes one MVT Feature message DIRECTLY into an OrcMaps Feature, with no
// intermediate MvtFeature.
//
// This exists for memory, not tidiness. A dense feature's geometry dominates
// everything else: a 36,259-byte encoded feature is roughly 9,000 coordinate
// pairs, about 72 KiB once decoded, and building an MvtFeature and then
// copying it into a Feature paid that twice -- ~144 KiB for one feature on a
// board with ~129 KiB of free heap. Decoding straight into the destination
// halves it.
//
// `layer` supplies extent and name; its own features are not read.
bool DecodeFeatureAsFeature(const uint8_t* data, size_t length,
                            const std::vector<std::string>& layer_keys,
                            const std::vector<MvtValue>& layer_values,
                            const MvtLayer& layer,
                            FeatureParseScratch* scratch, Feature* out);

}  // namespace internal
}  // namespace orcmap
