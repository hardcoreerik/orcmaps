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

}  // namespace orcmap
