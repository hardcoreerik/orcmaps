#pragma once

#include "orcmap/pack_discovery.hpp"

namespace orcmap {
namespace host {

// PackFileSystem for host tools and tests, using std::filesystem. Same
// contract as the ESP-IDF adapter: one directory, no recursion.
class PackFileSystem final : public orcmap::PackFileSystem {
 public:
  bool ListManifestNames(const std::string& dir,
                         std::vector<std::string>* out) override;
  bool ReadTextFile(const std::string& path, size_t max_bytes,
                    std::string* out) override;
  bool FileSize(const std::string& path, uint64_t* size) override;
};

}  // namespace host
}  // namespace orcmap
