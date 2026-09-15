#pragma once

#include "orcmap/pack_discovery.hpp"

namespace orcmap {
namespace esp_idf {

// PackFileSystem over the C library's directory and file calls, which on
// ESP-IDF reach whatever VFS the application mounted (FATFS on SD, SPIFFS,
// LittleFS -- discovery does not care and does not mount anything itself).
//
// Lists exactly one directory and never recurses. Nothing here includes
// M5GFX, M5Unified, or any display header.
class PackFileSystem final : public orcmap::PackFileSystem {
 public:
  bool ListManifestNames(const std::string& dir,
                         std::vector<std::string>* out) override;
  bool ReadTextFile(const std::string& path, size_t max_bytes,
                    std::string* out) override;
  bool FileSize(const std::string& path, uint64_t* size) override;
};

}  // namespace esp_idf
}  // namespace orcmap
