#include "pack_filesystem.hpp"

#include <cstdio>
#include <filesystem>
#include <system_error>

namespace orcmap {
namespace host {
namespace {

constexpr char kManifestSuffix[] = ".manifest.json";

bool EndsWithManifestSuffix(const std::string& text) {
  const size_t n = sizeof(kManifestSuffix) - 1;
  return text.size() >= n && text.compare(text.size() - n, n, kManifestSuffix) == 0;
}

}  // namespace

bool PackFileSystem::ListManifestNames(const std::string& dir,
                                       std::vector<std::string>* out) {
  if (out == nullptr) return false;
  out->clear();
  std::error_code ec;
  // directory_iterator, not recursive_directory_iterator: one level only.
  std::filesystem::directory_iterator it(dir, ec);
  if (ec) return false;
  for (const std::filesystem::directory_entry& entry : it) {
    if (!entry.is_regular_file(ec) || ec) continue;
    const std::string name = entry.path().filename().string();
    if (name.empty() || name[0] == '.') continue;
    if (!EndsWithManifestSuffix(name)) continue;
    out->push_back(name);
  }
  return true;
}

bool PackFileSystem::ReadTextFile(const std::string& path, size_t max_bytes,
                                  std::string* out) {
  if (out == nullptr) return false;
  out->clear();
  std::FILE* file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) return false;
  std::string text;
  char buffer[4096];
  for (;;) {
    const size_t got = std::fread(buffer, 1, sizeof(buffer), file);
    if (got == 0) break;
    if (text.size() + got > max_bytes) {
      std::fclose(file);
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
  std::error_code ec;
  const std::filesystem::path p(path);
  if (!std::filesystem::is_regular_file(p, ec) || ec) return false;
  const auto bytes = std::filesystem::file_size(p, ec);
  if (ec) return false;
  if (size != nullptr) *size = static_cast<uint64_t>(bytes);
  return true;
}

}  // namespace host
}  // namespace orcmap
