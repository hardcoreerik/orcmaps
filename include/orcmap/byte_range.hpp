#pragma once

#include <cstddef>
#include <cstdint>

namespace orcmap {

// Range arithmetic shared by every ByteSource adapter.
//
// Each adapter has to answer the same question before it touches its
// backing store: given a request for `length` bytes at `offset`, how much
// may actually be read from a source of `size` bytes? Getting that wrong in
// a flash-backed adapter reads outside the partition, so the arithmetic
// lives in one header-only place that host tests can exercise without an
// ESP-IDF toolchain or a real device.
//
// Clamping rather than rejecting a partly-satisfiable read matches the
// ByteSource contract, which documents a short read as legitimate and makes
// the caller check the returned count.

// Clamps [offset, offset + length) to a source of `size` bytes.
//
// On success `*out_length` is the number of bytes that may be read, which is
// at most `length` and never extends past `size`. Returns false, with
// `*out_length` set to 0, when nothing can be read: a zero-length request,
// or an offset at or past the end.
//
// The sum offset + length is never formed, so a hostile offset near the top
// of the 64-bit range cannot wrap around into a range that looks valid.
inline bool ClampReadRange(uint64_t offset, size_t length, uint64_t size,
                           size_t* out_length) {
  if (out_length == nullptr) return false;
  *out_length = 0;
  if (length == 0) return false;
  if (offset >= size) return false;
  const uint64_t remaining = size - offset;
  *out_length = remaining < static_cast<uint64_t>(length)
                    ? static_cast<size_t>(remaining)
                    : length;
  return *out_length != 0;
}

}  // namespace orcmap
