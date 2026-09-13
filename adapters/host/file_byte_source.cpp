#include "file_byte_source.hpp"

namespace orcmap::host {

FileByteSource::FileByteSource(const std::string& path) {
  file_ = std::fopen(path.c_str(), "rb");
  if (file_ == nullptr) return;
  if (std::fseek(file_, 0, SEEK_END) != 0) {
    std::fclose(file_);
    file_ = nullptr;
    return;
  }
  const long end = std::ftell(file_);
  if (end < 0) {
    std::fclose(file_);
    file_ = nullptr;
    return;
  }
  size_ = static_cast<uint64_t>(end);
  std::fseek(file_, 0, SEEK_SET);
}

FileByteSource::~FileByteSource() {
  if (file_ != nullptr) std::fclose(file_);
}

size_t FileByteSource::Read(uint64_t offset, void* destination, size_t length) {
  if (file_ == nullptr) return 0;
  if (std::fseek(file_, static_cast<long>(offset), SEEK_SET) != 0) return 0;
  return std::fread(destination, 1, length, file_);
}

}  // namespace orcmap::host
