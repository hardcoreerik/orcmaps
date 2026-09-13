#pragma once

#include <cstddef>
#include <cstdint>

namespace orcmap {

// Abstract random-access byte source: an SD file, a host file (tests/
// tools), OrcSDR's own storage layer, or eventually an HTTP Range source.
// This is the one seam that keeps the tile/archive core free of any
// filesystem or platform dependency -- see docs/ARCHITECTURE.md.
//
// A ByteSource does not own the underlying resource's lifetime beyond what
// its concrete adapter documents; callers must keep it alive for as long
// as any reader built on top of it (e.g. PmTilesReader) is in use.
class ByteSource {
 public:
  virtual ~ByteSource() = default;

  // Reads up to `length` bytes starting at `offset` into `destination`.
  // Returns the number of bytes actually read; a short read (including 0)
  // signals EOF or an I/O error -- callers must check the return value,
  // this never throws and never reads out of bounds of what's available.
  virtual size_t Read(uint64_t offset, void* destination, size_t length) = 0;

  // Total size in bytes. Returns 0 if unknown (callers should treat 0 as
  // "unavailable", not "empty file", if `Valid()` is also false).
  virtual uint64_t Size() const = 0;

  // Whether this source is usable (e.g. the underlying file opened).
  virtual bool Valid() const = 0;
};

}  // namespace orcmap
