#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace orcmap {

// Compression codes match the PMTiles v3 header field. This enum lives
// here (not in pmtiles.hpp) because payload compression is a data-layer
// concern independent of the archive format.

enum class Compression : uint8_t {
  kUnknown = 0,
  kNone = 1,
  kGzip = 2,
  kBrotli = 3,
  kZstd = 4,
};

// Decompresses `input` into `output` without exceeding `max_output_size`
// bytes. kNone copies; kGzip inflates a full RFC 1952 gzip wrapper.
// kBrotli, kZstd, and kUnknown return false (unsupported).
//
// On failure `output` is cleared. Never throws.
//
// Gzip: structure (magic, CM=deflate, header/trailer presence) is
// checked; ISIZE (uncompressed size mod 2^32) must be <= max_output_size;
// CRC32 of the decompressed bytes is verified with vendored miniz
// mz_crc32. Header CRC (FHCRC) is skipped, not verified. Reserved flag
// bits are not rejected. ISIZE==0 with a non-empty deflate body fails
// (4 GiB wraparound is not supported). Output is produced with
// tinfl_decompress_mem_to_mem into a buffer of size ISIZE (never
// tinfl_decompress_mem_to_heap).

bool DecompressPayload(Compression compression, const uint8_t* input,
                       size_t input_size, size_t max_output_size,
                       std::vector<uint8_t>* output);

}  // namespace orcmap
