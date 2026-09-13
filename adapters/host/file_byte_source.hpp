#pragma once

#include <cstdio>
#include <string>

#include "orcmap/byte_source.hpp"

namespace orcmap::host {

// Standard-C-stdio-backed ByteSource for host tests and command-line tools
// (tools/pack-inspect, tools/pack-verify) that run on a PC, not on-device.
// Not used by the ESP32/OrcSDR build -- see adapters/esp_idf for that.
class FileByteSource : public ByteSource {
 public:
  explicit FileByteSource(const std::string& path);
  ~FileByteSource() override;

  FileByteSource(const FileByteSource&) = delete;
  FileByteSource& operator=(const FileByteSource&) = delete;

  size_t Read(uint64_t offset, void* destination, size_t length) override;
  uint64_t Size() const override { return size_; }
  bool Valid() const override { return file_ != nullptr; }

 private:
  std::FILE* file_ = nullptr;
  uint64_t size_ = 0;
};

}  // namespace orcmap::host
