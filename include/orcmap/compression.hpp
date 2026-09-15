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

// Forward-only view of an inflating payload: bytes are produced on demand
// through a 32 KiB LZ77 window and consumed as they appear, so a caller can
// walk a compressed tile without ever holding the inflated tile.
//
// This is what makes a dense tile renderable on a board whose largest free
// block is smaller than the inflated tile. Measured on a CYD 3.5":
// 69,632-byte largest block against a 111,366-byte inflated Oregon z7 tile.
// The whole scratch below is ~43 KiB and is reusable, so the steady state
// needs no large contiguous allocation at all.
//
// Forward-only by construction: there is no seek. A parser needing two
// passes calls Begin() again, which re-inflates from the start.
class InflatingByteStream {
 public:
  // Allocates the window, inflater state and input chunk on first use and
  // keeps them for every subsequent Begin(), so repeated tiles do not
  // re-allocate. Returns false if the scratch cannot be allocated.
  bool Begin(Compression compression, CompressedChunkReader reader, void* ctx,
             uint64_t input_offset, size_t input_size);

  // Exact reads/skips over the inflated byte sequence. False means the
  // stream ended early or the payload is malformed -- never a short read.
  bool Read(uint8_t* dst, size_t count);
  bool Skip(uint64_t count);
  bool ReadVarint(uint64_t* value);
  bool AtEnd();

  // Inflated bytes consumed so far.
  uint64_t consumed() const { return consumed_; }

 private:
  bool Fill();  // produce the next run of inflated bytes into the window

  Compression compression_ = Compression::kUnknown;
  CompressedChunkReader reader_ = nullptr;
  void* ctx_ = nullptr;
  uint64_t input_offset_ = 0;
  size_t input_size_ = 0;      // compressed bytes of the deflate stream
  size_t input_consumed_ = 0;
  uint64_t consumed_ = 0;
  uint64_t expected_output_ = 0;

  std::vector<uint8_t> window_;   // 32 KiB LZ77 ring, also the output buffer
  std::vector<uint8_t> state_;    // tinfl_decompressor
  std::vector<uint8_t> chunk_;    // compressed input staging
  size_t window_pos_ = 0;         // ring write position
  size_t avail_start_ = 0;        // unconsumed run start within window_
  size_t avail_len_ = 0;
  bool finished_ = false;
  bool failed_ = false;
};

bool DecompressPayload(Compression compression, const uint8_t* input,
                       size_t input_size, size_t max_output_size,
                       std::vector<uint8_t>* output);

}  // namespace orcmap
