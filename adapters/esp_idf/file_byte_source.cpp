#include "orcmap/esp_idf/file_byte_source.hpp"

#include <limits>
#include <sys/types.h>

namespace orcmap {
namespace esp_idf {

namespace {

bool OffsetFitsOff(uint64_t offset) {
  return offset <= static_cast<uint64_t>(std::numeric_limits<off_t>::max());
}

}  // namespace

FileByteSource::FileByteSource(const char* path) {
  if (path == nullptr) return;
  file_ = std::fopen(path, "rb");
  if (file_ == nullptr) return;
  // Default libc buffering. Do not setvbuf(..., _IONBF, ...).
  if (fseeko(file_, 0, SEEK_END) != 0) {
    std::fclose(file_);
    file_ = nullptr;
    return;
  }
  const off_t end = ftello(file_);
  if (end < 0) {
    std::fclose(file_);
    file_ = nullptr;
    return;
  }
  size_ = static_cast<uint64_t>(end);
  fseeko(file_, 0, SEEK_SET);
}

FileByteSource::~FileByteSource() {
  if (file_ != nullptr) std::fclose(file_);
}

bool FileByteSource::Seek(uint64_t offset) {
  if (file_ == nullptr) return false;
  if (!OffsetFitsOff(offset)) return false;
  return fseeko(file_, static_cast<off_t>(offset), SEEK_SET) == 0;
}

size_t FileByteSource::Read(uint64_t offset, void* destination, size_t length) {
  if (file_ == nullptr || destination == nullptr) return 0;
  if (!Seek(offset)) return 0;
  const size_t n = std::fread(destination, 1, length, file_);
  bytes_read_ += n;
  return n;
}

}  // namespace esp_idf
}  // namespace orcmap
