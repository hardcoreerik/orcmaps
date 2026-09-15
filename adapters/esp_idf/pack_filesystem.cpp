#include "orcmap/esp_idf/pack_filesystem.hpp"

#include <dirent.h>
#include <sys/stat.h>

#include <cstdio>

namespace orcmap {
namespace esp_idf {
namespace {

constexpr char kManifestSuffix[] = ".manifest.json";

bool EndsWithManifestSuffix(const char* name) {
  const std::string text(name);
  const size_t n = sizeof(kManifestSuffix) - 1;
  return text.size() >= n && text.compare(text.size() - n, n, kManifestSuffix) == 0;
}

}  // namespace

bool PackFileSystem::ListManifestNames(const std::string& dir,
                                       std::vector<std::string>* out) {
  if (out == nullptr) return false;
  out->clear();
  DIR* handle = ::opendir(dir.c_str());
  if (handle == nullptr) return false;
  // One directory, one pass. Subdirectories are ignored rather than walked:
  // discovery must never traverse an arbitrary filesystem tree.
  while (const dirent* entry = ::readdir(handle)) {
    if (entry->d_name[0] == '.') continue;
    if (!EndsWithManifestSuffix(entry->d_name)) continue;
    out->push_back(entry->d_name);
  }
  ::closedir(handle);
  return true;
}

bool PackFileSystem::ReadTextFile(const std::string& path, size_t max_bytes,
                                  std::string* out) {
  if (out == nullptr) return false;
  out->clear();
  std::FILE* file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) return false;

  std::string text;
  char buffer[512];
  for (;;) {
    const size_t got = std::fread(buffer, 1, sizeof(buffer), file);
    if (got == 0) break;
    if (text.size() + got > max_bytes) {
      // Refuse rather than truncate: a truncated manifest would parse as
      // malformed and report the wrong reason.
      std::fclose(file);
      out->clear();
      return false;
    }
    text.append(buffer, got);
  }
  const bool failed = std::ferror(file) != 0;
  std::fclose(file);
  if (failed) return false;
  *out = std::move(text);
  return true;
}

bool PackFileSystem::FileSize(const std::string& path, uint64_t* size) {
  struct stat info {};
  if (::stat(path.c_str(), &info) != 0) return false;
  if (!S_ISREG(info.st_mode)) return false;
  if (size != nullptr) *size = static_cast<uint64_t>(info.st_size);
  return true;
}

}  // namespace esp_idf
}  // namespace orcmap
