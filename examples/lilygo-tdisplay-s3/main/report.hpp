#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace orcmap_demo {

struct Report {
  int display_width;
  int display_height;
  const char* style_id;
  size_t visible;
  size_t present;
  size_t missing;
  double enumerate_ms;
  double lookup_ms;
  double gzip_ms;
  double decode_ms;
  double translate_ms;
  double classify_ms;
  double render_ms;
  double frame_total_ms;
  double power_to_map_ms;
  uint64_t bytes_read;
  size_t internal_before;
  size_t internal_min;
  size_t psram_before;
  size_t psram_min;
  size_t psram_total;
};

inline bool WriteReport(const char* path, const Report& report) {
  FILE* file = std::fopen(path, "w");
  if (file == nullptr) return false;

  const int written = std::fprintf(
      file,
      "ORCMAPS LILYGO T-DISPLAY-S3 HARDWARE DEMO\n"
      "display=%dx%d\n"
      "map=/sd/orcmaps/springfield.pmtiles\n"
      "zoom=14 style=%s\n"
      "visible=%zu present=%zu missing=%zu\n"
      "timing_ms enumerate=%.3f GetTile=%.3f gzip=%.3f decode=%.3f "
      "translate=%.3f classify=%.3f render=%.3f frame_total=%.3f\n"
      "power_to_map_ms=%.3f\n"
      "bytes_read=%llu\n"
      "memory internal_before=%zu internal_min=%zu psram_before=%zu "
      "psram_min=%zu psram_total=%zu\n"
      "RESULT=PASS\n",
      report.display_width, report.display_height, report.style_id,
      report.visible,
      report.present, report.missing, report.enumerate_ms, report.lookup_ms,
      report.gzip_ms, report.decode_ms, report.translate_ms,
      report.classify_ms, report.render_ms, report.frame_total_ms,
      report.power_to_map_ms,
      static_cast<unsigned long long>(report.bytes_read),
      report.internal_before, report.internal_min, report.psram_before,
      report.psram_min, report.psram_total);
  const bool ok = written >= 0 && std::fflush(file) == 0;
  return std::fclose(file) == 0 && ok;
}

inline bool WriteNextReport(const char* directory, const Report& report,
                            char* saved_path, size_t saved_path_size) {
  for (unsigned number = 1; number <= 9999; ++number) {
    const int length = std::snprintf(saved_path, saved_path_size,
                                     "%s/orcmaps-report-%04u.txt", directory,
                                     number);
    if (length < 0 || static_cast<size_t>(length) >= saved_path_size) {
      return false;
    }
    FILE* existing = std::fopen(saved_path, "rb");
    if (existing == nullptr) return WriteReport(saved_path, report);
    std::fclose(existing);
  }
  return false;
}

inline void FormatResultLine(char* output, size_t output_size,
                             double total_ms, bool report_saved) {
  std::snprintf(output, output_size,
                report_saved ? "Result: %.0f ms  Report saved"
                             : "Result: %.0f ms  SAVE FAILED",
                total_ms);
}

}  // namespace orcmap_demo
