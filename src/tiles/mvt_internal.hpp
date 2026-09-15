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

}  // namespace internal
}  // namespace orcmap
