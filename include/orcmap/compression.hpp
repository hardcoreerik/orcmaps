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

// Pull-based compressed input. Returns bytes actually read (short read ==
// failure to the caller). Lets the inflater take the payload in small
// pieces instead of requiring it all in RAM.
using CompressedChunkReader = size_t (*)(void* ctx, uint64_t offset,
                                         uint8_t* dst, size_t len);

// STREAMING inflate: reads the compressed payload through `reader` a chunk at
// a time, so the caller never holds the compressed bytes and the inflated
// bytes simultaneously.
//
// This exists because contiguous memory, not total memory, is the binding
// limit on a board without PSRAM. Measured on a CYD 3.5" (ESP32-3248S035):
// 211,788 bytes free but a largest free block of only 106,496, while one
// Oregon z7 tile inflates to 111,366 bytes -- so holding an 82 KiB
// compressed buffer *and* a 111 KiB output buffer was impossible, and even
// the output alone did not fit once the heap had fragmented.
//
// `output` is resized to the exact inflated size, which gzip's ISIZE trailer
// states up front. Reserve its capacity once, early, and reuse it across
// tiles: that keeps the single large allocation out of the steady state.
//
// Only kNone and kGzip are supported, matching DecompressPayload.
bool DecompressStreaming(Compression compression, CompressedChunkReader reader,
                         void* ctx, uint64_t input_offset, size_t input_size,
                         size_t max_output_size, std::vector<uint8_t>* output);

bool DecompressPayload(Compression compression, const uint8_t* input,
                       size_t input_size, size_t max_output_size,
                       std::vector<uint8_t>* output);

}  // namespace orcmap
