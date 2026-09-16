#pragma once

#include <cstddef>
#include <cstdint>

#include "orcmap/byte_source.hpp"

namespace orcmap {
namespace esp_idf {

// ByteSource over a read-only ESP-IDF flash partition.
//
// This exists so an application can ship map data inside its own firmware
// image instead of requiring removable storage. Its entire responsibility is
// to turn a named flash partition into readable bytes; it knows nothing about
// PMTiles, tiles, rendering, or which application is using it. The partition
// is identified by a label the caller supplies at runtime, so no partition
// name is ever compiled into OrcMaps.
//
// Because it satisfies ByteSource, the normal pipeline applies unchanged:
//
//     flash partition -> PartitionByteSource -> PmTilesReader -> gzip/MVT
//         -> FeatureTile -> renderer -> RenderTarget
//
// There is deliberately no second embedded-map code path and no firmware-
// specific map format: the bytes in flash are an ordinary PMTiles archive.
//
// Read-only by construction. This class never erases or writes, and a
// partition holding it should be treated as an immutable asset replaced
// wholesale by a firmware or map update -- not as writable user storage.
// Anything belonging to the user (a chosen location, markers, notes) is the
// application's to persist elsewhere.
//
// Implementation note: the partition handle is held as a void* rather than
// an esp_partition_t*, because esp_partition_t is a typedef of an anonymous
// struct and so cannot be forward declared. Including esp_partition.h here
// would push that dependency onto every consumer of this header and force
// the component's public REQUIRES to grow. The type therefore appears at no
// API boundary and is cast only inside the translation unit that already
// includes the ESP-IDF header.

class PartitionByteSource : public ByteSource {
 public:
  // Finds a data partition by label. Valid() is false if no such partition
  // exists, which is the normal state on a device whose flash was written
  // without the map image -- callers should fall back rather than fail.
  explicit PartitionByteSource(const char* label);

  PartitionByteSource(const PartitionByteSource&) = delete;
  PartitionByteSource& operator=(const PartitionByteSource&) = delete;

  // Reads up to `length` bytes at `offset`. A request that runs past the end
  // of the partition is clamped to what remains; one starting at or past the
  // end reads nothing. Returns the number of bytes read.
  size_t Read(uint64_t offset, void* destination, size_t length) override;

  // Size of the partition, not of the data written into it. A 6 MiB
  // partition holding a 4.6 MiB archive reports 6 MiB; PMTiles bounds its own
  // reads by its header, so the trailing bytes are never parsed.
  uint64_t Size() const override { return size_; }
  bool Valid() const override { return partition_ != nullptr; }

  // Total bytes handed back by Read, for reporting I/O volume the same way
  // FileByteSource does.
  uint64_t BytesRead() const { return bytes_read_; }

 private:
  const void* partition_ = nullptr;
  uint64_t size_ = 0;
  uint64_t bytes_read_ = 0;
};

}  // namespace esp_idf
}  // namespace orcmap
