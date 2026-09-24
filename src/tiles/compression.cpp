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
  // Same work as tinfl_decompress_mem_to_mem, but with the decompressor on
  // the heap: it is ~8 KiB (8,364 B on RV32), which would otherwise have to
  // fit on the calling task's stack -- internal RAM on an ESP32 -- for every
  // PMTiles directory read. InflatingByteStream holds its state the same way.
  std::vector<uint8_t> state(sizeof(tinfl_decompressor));
  tinfl_decompressor* decompressor =
      reinterpret_cast<tinfl_decompressor*>(state.data());
  tinfl_init(decompressor);
  size_t in_len = deflate_len;
  size_t written = output->size();
  const tinfl_status status = tinfl_decompress(
      decompressor, deflate_start, &in_len, output->data(), output->data(),
      &written, TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
  if (status != TINFL_STATUS_DONE || written != isize) {
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

// Largest gzip header this reader will parse. Real PMTiles payloads (written
// by go-pmtiles / Planetiler) carry no FNAME/FCOMMENT, so 10 bytes is the
// norm; 512 leaves generous room while keeping the probe read small.
constexpr size_t kMaxGzipHeaderProbe = 512;
// Compressed input is pulled in pieces this size. Small enough that it never
// competes with the output buffer for a large contiguous block.
constexpr size_t kInflateChunkBytes = 2048;

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

// --- InflatingByteStream -------------------------------------------------

bool InflatingByteStream::Reserve() {
  if (window_.size() != TINFL_LZ_DICT_SIZE) window_.resize(TINFL_LZ_DICT_SIZE);
  if (state_.size() < sizeof(tinfl_decompressor)) {
    state_.resize(sizeof(tinfl_decompressor));
  }
  if (chunk_.size() < kInflateChunkBytes) chunk_.resize(kInflateChunkBytes);
  return window_.size() == TINFL_LZ_DICT_SIZE &&
         state_.size() >= sizeof(tinfl_decompressor) &&
         chunk_.size() >= kInflateChunkBytes;
}

bool InflatingByteStream::Begin(Compression compression,
                                CompressedChunkReader reader, void* ctx,
                                uint64_t input_offset, size_t input_size) {
  compression_ = compression;
  reader_ = reader;
  ctx_ = ctx;
  consumed_ = 0;
  input_consumed_ = 0;
  window_pos_ = 0;
  avail_start_ = 0;
  avail_len_ = 0;
  finished_ = false;
  failed_ = false;
  expected_output_ = 0;
  if (reader == nullptr) return false;

  if (compression == Compression::kNone) {
    input_offset_ = input_offset;
    input_size_ = input_size;
    expected_output_ = input_size;
    // Still buffered in chunks, so an uncompressed tile costs no more
    // contiguous memory than a compressed one.
    if (chunk_.size() < kInflateChunkBytes) chunk_.resize(kInflateChunkBytes);
    return true;
  }
  if (compression != Compression::kGzip) return false;
  if (input_size < 18) return false;

  // Header and trailer, exactly as DecompressStreaming does.
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
  if (pos > probe_len || pos + 8 > input_size) return false;

  uint8_t trailer[8];
  if (reader(ctx, input_offset + input_size - 8, trailer, 8) != 8) return false;
  expected_output_ = ReadU32LE(trailer + 4);

  input_offset_ = input_offset + pos;
  input_size_ = input_size - pos - 8;

  // Window must be exactly the LZ77 dictionary size: tinfl uses it as both
  // the output buffer and the back-reference dictionary when wrapping.
  if (window_.size() != TINFL_LZ_DICT_SIZE) window_.resize(TINFL_LZ_DICT_SIZE);
  if (state_.size() < sizeof(tinfl_decompressor)) {
    state_.resize(sizeof(tinfl_decompressor));
  }
  if (chunk_.size() < kInflateChunkBytes) chunk_.resize(kInflateChunkBytes);
  if (window_.size() != TINFL_LZ_DICT_SIZE ||
      state_.size() < sizeof(tinfl_decompressor)) {
    return false;
  }
  tinfl_init(reinterpret_cast<tinfl_decompressor*>(state_.data()));
  return true;
}

bool InflatingByteStream::Fill() {
  if (failed_) return false;
  if (avail_len_ > 0) return true;
  if (finished_) return false;

  if (compression_ == Compression::kNone) {
    const size_t remaining = input_size_ - input_consumed_;
    if (remaining == 0) {
      finished_ = true;
      return false;
    }
    const size_t want = remaining < chunk_.size() ? remaining : chunk_.size();
    if (reader_(ctx_, input_offset_ + input_consumed_, chunk_.data(), want) !=
        want) {
      failed_ = true;
      return false;
    }
    input_consumed_ += want;
    // For the uncompressed case the staging buffer IS the available run.
    avail_start_ = 0;
    avail_len_ = want;
    // Point reads at chunk_ by copying into the window instead of
    // special-casing every read site.
    if (window_.size() < chunk_.size()) window_.resize(chunk_.size());
    std::memcpy(window_.data(), chunk_.data(), want);
    return true;
  }

  tinfl_decompressor* inflater =
      reinterpret_cast<tinfl_decompressor*>(state_.data());
  for (;;) {
    const size_t remaining = input_size_ - input_consumed_;
    const size_t want = remaining < chunk_.size() ? remaining : chunk_.size();
    if (want > 0 && reader_(ctx_, input_offset_ + input_consumed_,
                            chunk_.data(), want) != want) {
      failed_ = true;
      return false;
    }
    size_t in_avail = want;
    size_t out_avail = TINFL_LZ_DICT_SIZE - window_pos_;
    const tinfl_status status = tinfl_decompress(
        inflater, chunk_.data(), &in_avail, window_.data(),
        window_.data() + window_pos_, &out_avail,
        remaining > want ? TINFL_FLAG_HAS_MORE_INPUT : 0);
    input_consumed_ += in_avail;
    if (out_avail > 0) {
      avail_start_ = window_pos_;
      avail_len_ = out_avail;
      window_pos_ = (window_pos_ + out_avail) & (TINFL_LZ_DICT_SIZE - 1);
    }
    if (status == TINFL_STATUS_DONE) finished_ = true;
    if (status < TINFL_STATUS_DONE) {  // negative values are failures
      failed_ = true;
      return false;
    }
    if (avail_len_ > 0) return true;
    if (finished_) return false;
    // No progress at all means a malformed stream rather than "needs more".
    if (in_avail == 0 && out_avail == 0) {
      failed_ = true;
      return false;
    }
  }
}

bool InflatingByteStream::Read(uint8_t* dst, size_t count) {
  size_t done = 0;
  while (done < count) {
    if (avail_len_ == 0 && !Fill()) return false;
    const size_t take = (count - done) < avail_len_ ? (count - done) : avail_len_;
    if (dst != nullptr) std::memcpy(dst + done, window_.data() + avail_start_, take);
    avail_start_ += take;
    avail_len_ -= take;
    consumed_ += take;
    done += take;
  }
  return true;
}

bool InflatingByteStream::Skip(uint64_t count) {
  while (count > 0) {
    if (avail_len_ == 0 && !Fill()) return false;
    const uint64_t take = count < avail_len_ ? count : avail_len_;
    avail_start_ += static_cast<size_t>(take);
    avail_len_ -= static_cast<size_t>(take);
    consumed_ += take;
    count -= take;
  }
  return true;
}

bool InflatingByteStream::ReadVarint(uint64_t* value) {
  if (value == nullptr) return false;
  uint64_t result = 0;
  int shift = 0;
  for (;;) {
    uint8_t byte = 0;
    if (!Read(&byte, 1)) return false;
    result |= static_cast<uint64_t>(byte & 0x7f) << shift;
    if ((byte & 0x80) == 0) break;
    shift += 7;
    if (shift > 63) return false;
  }
  *value = result;
  return true;
}

bool InflatingByteStream::AtEnd() {
  if (avail_len_ > 0) return false;
  if (failed_) return true;
  return !Fill();
}

}  // namespace orcmap
