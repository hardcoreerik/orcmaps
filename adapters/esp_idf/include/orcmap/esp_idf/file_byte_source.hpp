#pragma once

#include <cstdint>
#include <cstdio>

#include "orcmap/byte_source.hpp"

namespace orcmap {
namespace esp_idf {

// FILE*-backed ByteSource for an already-mounted ESP-IDF VFS path.
// Does not mount SD, does not know Tab5, PMTiles, or M5GFX.
//
// I/O policy: one fopen for the object's lifetime, libc default buffering
// (NOT _IONBF). OrcSDR's writable-file unbuffered policy is intentionally
// not used. Reads are fread after an offset seek.
//
// Offsets: fseeko/ftello when available; otherwise fseek/ftell. On
// ESP-IDF newlib, off_t/long is typically 32-bit, so offsets that do not
// fit fail cleanly (Read returns 0). Springfield packs are a few MiB.

class FileByteSource : public ByteSource {
 public:
  explicit FileByteSource(const char* path);
  ~FileByteSource() override;

  FileByteSource(const FileByteSource&) = delete;
  FileByteSource& operator=(const FileByteSource&) = delete;

  size_t Read(uint64_t offset, void* destination, size_t length) override;
  uint64_t Size() const override { return size_; }
  bool Valid() const override { return file_ != nullptr; }
  uint64_t BytesRead() const { return bytes_read_; }

 private:
  bool Seek(uint64_t offset);

  std::FILE* file_ = nullptr;
  uint64_t size_ = 0;
  uint64_t bytes_read_ = 0;
};

}  // namespace esp_idf
}  // namespace orcmap
