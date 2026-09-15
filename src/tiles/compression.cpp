#include "orcmap/compression.hpp"

#include <cstring>

#include "miniz.h"
#include "miniz_tinfl.h"

namespace orcmap {

namespace {

uint32_t ReadU32LE(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

bool StripGzipWrapper(const uint8_t* data, size_t len, const uint8_t** deflate_start,
                      size_t* deflate_len, uint32_t* crc32, uint32_t* isize) {
  if (len < 18 || data[0] != 0x1f || data[1] != 0x8b || data[2] != 0x08) {
    return false;
  }
  uint8_t flg = data[3];
  size_t pos = 10;
  if (flg & 0x04) {
    if (pos + 2 > len) return false;
    uint16_t xlen = static_cast<uint16_t>(data[pos]) |
                    (static_cast<uint16_t>(data[pos + 1]) << 8);
    pos += 2 + xlen;
  }
  if (flg & 0x08) {
    while (pos < len && data[pos] != 0) ++pos;
    ++pos;
  }
  if (flg & 0x10) {
    while (pos < len && data[pos] != 0) ++pos;
    ++pos;
  }
  if (flg & 0x02) pos += 2;
  if (pos + 8 > len) return false;
  *deflate_start = data + pos;
  *deflate_len = len - pos - 8;
  *crc32 = ReadU32LE(data + len - 8);
  *isize = ReadU32LE(data + len - 4);
  return true;
}

}  // namespace

bool DecompressPayload(Compression compression, const uint8_t* input,
                       size_t input_size, size_t max_output_size,
                       std::vector<uint8_t>* output) {
  if (output == nullptr) return false;
  output->clear();
  if (compression == Compression::kNone) {
    if (input_size == 0) return true;
    if (input == nullptr || input_size > max_output_size) return false;
    output->assign(input, input + input_size);
    return true;
  }
  if (compression != Compression::kGzip) return false;
  if (input == nullptr) return false;

  const uint8_t* deflate_start = nullptr;
  size_t deflate_len = 0;
  uint32_t expect_crc = 0;
  uint32_t isize = 0;
  if (!StripGzipWrapper(input, input_size, &deflate_start, &deflate_len,
                        &expect_crc, &isize)) {
    return false;
  }
  if (static_cast<size_t>(isize) > max_output_size) return false;

  if (isize == 0) {
    if (deflate_len == 0) return true;
    return false;
  }

  output->resize(isize);
  const size_t written = tinfl_decompress_mem_to_mem(
      output->data(), output->size(), deflate_start, deflate_len,
      TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
  if (written == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED || written != isize) {
    output->clear();
    return false;
  }
  const uint32_t got_crc = static_cast<uint32_t>(
      mz_crc32(MZ_CRC32_INIT, output->data(), output->size()));
  if (got_crc != expect_crc) {
    output->clear();
    return false;
  }
  return true;
}

namespace {

// Largest gzip header this reader will parse. Real PMTiles payloads (written
// by go-pmtiles / Planetiler) carry no FNAME/FCOMMENT, so 10 bytes is the
// norm; 512 leaves generous room while keeping the probe read small.
constexpr size_t kMaxGzipHeaderProbe = 512;
// Compressed input is pulled in pieces this size. Small enough that it never
// competes with the output buffer for a large contiguous block.
constexpr size_t kInflateChunkBytes = 2048;

}  // namespace

bool DecompressStreaming(Compression compression, CompressedChunkReader reader,
                         void* ctx, uint64_t input_offset, size_t input_size,
                         size_t max_output_size, std::vector<uint8_t>* output) {
  if (output == nullptr || reader == nullptr) return false;
  output->clear();
  if (compression == Compression::kNone) {
    if (input_size == 0) return true;
    if (input_size > max_output_size) return false;
    output->resize(input_size);
    return reader(ctx, input_offset, output->data(), input_size) == input_size;
  }
  if (compression != Compression::kGzip) return false;
  if (input_size < 18) return false;

  // Header: probe the front, then locate where the deflate stream starts.
  uint8_t probe[kMaxGzipHeaderProbe];
  const size_t probe_len =
      input_size < kMaxGzipHeaderProbe ? input_size : kMaxGzipHeaderProbe;
  if (reader(ctx, input_offset, probe, probe_len) != probe_len) return false;
  if (probe[0] != 0x1f || probe[1] != 0x8b || probe[2] != 0x08) return false;
  const uint8_t flg = probe[3];
  size_t pos = 10;
  if (flg & 0x04) {
    if (pos + 2 > probe_len) return false;
    const uint16_t xlen = static_cast<uint16_t>(probe[pos]) |
                          (static_cast<uint16_t>(probe[pos + 1]) << 8);
    pos += 2 + xlen;
  }
  if (flg & 0x08) {
    while (pos < probe_len && probe[pos] != 0) ++pos;
    ++pos;
  }
  if (flg & 0x10) {
    while (pos < probe_len && probe[pos] != 0) ++pos;
    ++pos;
  }
  if (flg & 0x02) pos += 2;
  // A header longer than the probe is refused rather than guessed at.
  if (pos > probe_len || pos + 8 > input_size) return false;

  // Trailer: CRC-32 then ISIZE, which gives the exact output size.
  uint8_t trailer[8];
  if (reader(ctx, input_offset + input_size - 8, trailer, 8) != 8) return false;
  const uint32_t expect_crc = ReadU32LE(trailer);
  const uint32_t isize = ReadU32LE(trailer + 4);
  if (static_cast<size_t>(isize) > max_output_size) return false;

  const size_t deflate_len = input_size - pos - 8;
  if (isize == 0) return deflate_len == 0;

  output->resize(isize);

  // tinfl's state is ~11 KB -- far too large for an embedded task stack, so
  // it lives on the heap for the duration of this call.
  std::vector<uint8_t> state(sizeof(tinfl_decompressor));
  tinfl_decompressor* inflater =
      reinterpret_cast<tinfl_decompressor*>(state.data());
  tinfl_init(inflater);

  uint8_t chunk[kInflateChunkBytes];
  size_t consumed = 0;
  size_t produced = 0;
  for (;;) {
    const size_t want = deflate_len - consumed < kInflateChunkBytes
                            ? deflate_len - consumed
                            : kInflateChunkBytes;
    size_t in_avail = want;
    if (want > 0 &&
        reader(ctx, input_offset + pos + consumed, chunk, want) != want) {
      output->clear();
      return false;
    }
    const bool more_input = (consumed + want) < deflate_len;
    size_t out_avail = isize - produced;
    const tinfl_status status = tinfl_decompress(
        inflater, chunk, &in_avail, output->data(), output->data() + produced,
        &out_avail,
        TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF |
            (more_input ? TINFL_FLAG_HAS_MORE_INPUT : 0));
    consumed += in_avail;
    produced += out_avail;
    if (status == TINFL_STATUS_DONE) break;
    if (status != TINFL_STATUS_NEEDS_MORE_INPUT &&
        status != TINFL_STATUS_HAS_MORE_OUTPUT) {
      output->clear();
      return false;
    }
    // No forward progress means a malformed stream; refuse rather than spin.
    if (in_avail == 0 && out_avail == 0) {
      output->clear();
      return false;
    }
  }
  if (produced != isize) {
    output->clear();
    return false;
  }
  const uint32_t got_crc = static_cast<uint32_t>(
      mz_crc32(MZ_CRC32_INIT, output->data(), output->size()));
  if (got_crc != expect_crc) {
    output->clear();
    return false;
  }
  return true;
}

}  // namespace orcmap
