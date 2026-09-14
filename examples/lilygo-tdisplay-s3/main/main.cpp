// LilyGO T-Display-S3 Touch + SD Shield Springfield hardware demo.

#include <M5GFX.h>
#include <lgfx/v1/panel/Panel_ST7789.hpp>

#include "orcmap/compression.hpp"
#include "orcmap/esp_idf/file_byte_source.hpp"
#include "orcmap/experimental/mvt_classify.hpp"
#include "orcmap/feature.hpp"
#include "orcmap/m5gfx/display_target.hpp"
#include "orcmap/mvt.hpp"
#include "orcmap/mvt_translate.hpp"
#include "orcmap/pmtiles.hpp"
#include "orcmap/renderer.hpp"
#include "orcmap/style.hpp"
#include "orcmap/viewport.hpp"
#include "report.hpp"

#include <driver/gpio.h>
#include <driver/sdmmc_host.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_psram.h>
#include <esp_timer.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

constexpr char kTag[] = "orcmap_tdisplay_s3";
constexpr char kMapPath[] = "/sd/orcmaps/springfield.pmtiles";
constexpr char kReportDirectory[] = "/sd/orcmaps";
constexpr char kStylePath[] = "/sd/orcmaps/style.txt";
constexpr double kCenterLat = 44.0500;
constexpr double kCenterLon = -123.0220;
constexpr uint8_t kZoom = 14;
constexpr int kTileSizePx = 256;
constexpr size_t kDecompressBudget = 512u * 1024u;
constexpr size_t kBenchmarkBytes = 512u * 1024u;

// Board configuration derived from LilyGO's MIT-licensed T-Display-S3
// LovyanGFX example and official pin map.
class TDisplayS3 : public lgfx::LGFX_Device {
 public:
  TDisplayS3() {
    {
      auto config = bus_.config();
      config.pin_wr = 8;
      config.pin_rd = 9;
      config.pin_rs = 7;
      config.pin_d0 = 39;
      config.pin_d1 = 40;
      config.pin_d2 = 41;
      config.pin_d3 = 42;
      config.pin_d4 = 45;
      config.pin_d5 = 46;
      config.pin_d6 = 47;
      config.pin_d7 = 48;
      bus_.config(config);
      panel_.setBus(&bus_);
    }
    {
      auto config = panel_.config();
      config.pin_cs = 6;
      config.pin_rst = 5;
      config.pin_busy = -1;
      config.offset_rotation = 1;
      config.offset_x = 35;
      config.readable = false;
      config.invert = true;
      config.rgb_order = false;
      config.dlen_16bit = false;
      config.bus_shared = false;
      config.panel_width = 170;
      config.panel_height = 320;
      panel_.config(config);
    }
    {
      auto config = light_.config();
      config.pin_bl = 38;
      config.invert = false;
      config.freq = 22000;
      config.pwm_channel = 7;
      light_.config(config);
      panel_.setLight(&light_);
    }
    setPanel(&panel_);
  }

 private:
  lgfx::Bus_Parallel8 bus_;
  lgfx::Panel_ST7789 panel_;
  lgfx::Light_PWM light_;
};

TDisplayS3 g_display;
sdmmc_card_t* g_card = nullptr;

int64_t NowUs() { return esp_timer_get_time(); }

size_t InternalFree() {
  return heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
}
size_t InternalMin() {
  return heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
}
size_t InternalLargest() {
  return heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
}
size_t PsramFree() { return heap_caps_get_free_size(MALLOC_CAP_SPIRAM); }
size_t PsramMin() {
  return heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
}
size_t PsramLargest() {
  return heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
}

void StatusLine(int y, uint32_t color, const char* text) {
  g_display.setTextDatum(lgfx::top_left);
  g_display.setTextSize(1);
  g_display.setTextColor(color, TFT_BLACK);
  g_display.drawString(text, 6, y);
}

[[noreturn]] void Fail(const char* line, const char* detail) {
  ESP_LOGE(kTag, "%s: %s", line, detail ? detail : "");
  g_display.fillScreen(TFT_BLACK);
  StatusLine(8, TFT_RED, "ORCMAPS T-DISPLAY-S3 FAILED");
  StatusLine(34, TFT_WHITE, line);
  if (detail) StatusLine(52, TFT_ORANGE, detail);
  StatusLine(78, TFT_DARKGREY, "See USB serial log.");
  for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}

bool MountSd() {
  esp_vfs_fat_sdmmc_mount_config_t mount{};
  mount.format_if_mount_failed = false;
  mount.max_files = 8;
  mount.allocation_unit_size = 16 * 1024;

  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.max_freq_khz = SDMMC_FREQ_DEFAULT;

  sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
  slot.width = 1;
  slot.clk = GPIO_NUM_11;
  slot.cmd = GPIO_NUM_13;
  slot.d0 = GPIO_NUM_12;

  const esp_err_t err =
      esp_vfs_fat_sdmmc_mount("/sd", &host, &slot, &mount, &g_card);
  if (err != ESP_OK) {
    ESP_LOGE(kTag, "SD Shield mount failed: %s", esp_err_to_name(err));
    return false;
  }
  sdmmc_card_print_info(stdout, g_card);
  return true;
}

void BenchmarkSequentialRead(const char* path, uint64_t file_size) {
  FILE* file = std::fopen(path, "rb");
  if (file == nullptr) {
    ESP_LOGE(kTag, "benchmark fopen failed");
    return;
  }
  const size_t nbytes = static_cast<size_t>(
      file_size < kBenchmarkBytes ? file_size : kBenchmarkBytes);
  const size_t blocks[] = {4096, 16384, 32768, 65536};
  std::vector<uint8_t> buffer(65536);
  for (size_t block : blocks) {
    std::rewind(file);
    size_t got = 0;
    const int64_t started = NowUs();
    while (got < nbytes) {
      const size_t want = nbytes - got < block ? nbytes - got : block;
      const size_t count = std::fread(buffer.data(), 1, want, file);
      if (count == 0) break;
      got += count;
    }
    const double ms = static_cast<double>(NowUs() - started) / 1000.0;
    const double mbps =
        ms > 0.0 ? (static_cast<double>(got) / 1.0e6) / (ms / 1000.0) : 0.0;
    ESP_LOGI(kTag,
             "ORCMAPS_SD_READ block=%zu bytes=%zu elapsed_ms=%.3f "
             "mb_per_sec=%.3f",
             block, got, ms, mbps);
  }
  std::fclose(file);
}

const orcmap::MapStyle& LoadStyle() {
  FILE* file = std::fopen(kStylePath, "rb");
  if (file == nullptr) return orcmap::styles::OrcSdrDark();
  char id[64]{};
  const bool read = std::fgets(id, sizeof(id), file) != nullptr;
  std::fclose(file);
  if (!read) return orcmap::styles::OrcSdrDark();
  id[std::strcspn(id, "\r\n\t ")] = '\0';
  const orcmap::MapStyle* style = orcmap::FindBuiltinStyleById(id);
  if (style == nullptr) {
    ESP_LOGW(kTag, "unknown style '%s'; using orcsdr-dark", id);
    return orcmap::styles::OrcSdrDark();
  }
  return *style;
}

}  // namespace

extern "C" void app_main() {
  const int64_t power_started = NowUs();
  gpio_set_direction(GPIO_NUM_15, GPIO_MODE_OUTPUT);
  gpio_set_level(GPIO_NUM_15, 1);

  if (!g_display.init()) {
    ESP_LOGE(kTag, "display init failed");
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
  }
  g_display.setRotation(0);
  g_display.setBrightness(180);
  g_display.fillScreen(TFT_BLACK);
  StatusLine(6, TFT_WHITE, "ORCMAPS");
  StatusLine(22, TFT_DARKGREY, "T-Display-S3 Touch + SD Shield");

  const int display_width = g_display.width();
  const int display_height = g_display.height();
  char line[96];
  if (display_width != 320 || display_height != 170) {
    std::snprintf(line, sizeof(line), "got %dx%d", display_width,
                  display_height);
    Fail("Display not 320x170", line);
  }
  StatusLine(44, TFT_GREEN, "Display .... 320x170 OK");

  if (!esp_psram_is_initialized()) Fail("PSRAM", "not initialized");
  std::snprintf(line, sizeof(line), "PSRAM ...... %u KiB OK",
                static_cast<unsigned>(esp_psram_get_size() / 1024));
  StatusLine(60, TFT_GREEN, line);
  ESP_LOGI(kTag, "PSRAM total=%u free=%u largest=%u",
           static_cast<unsigned>(esp_psram_get_size()),
           static_cast<unsigned>(PsramFree()),
           static_cast<unsigned>(PsramLargest()));

  if (!MountSd()) Fail("SD Shield mount failed", "check shield/card");
  StatusLine(76, TFT_GREEN, "SD Shield .. OK");

  orcmap::esp_idf::FileByteSource source(kMapPath);
  if (!source.Valid()) Fail("Map missing", kMapPath);
  StatusLine(92, TFT_GREEN, "Map ........ OK");
  ESP_LOGI(kTag, "map %s size=%llu", kMapPath,
           static_cast<unsigned long long>(source.Size()));

  StatusLine(114, TFT_YELLOW, "Benchmarking SD...");
  BenchmarkSequentialRead(kMapPath, source.Size());
  StatusLine(130, TFT_YELLOW, "Rendering Springfield...");

  orcmap::PmTilesReader reader(&source);
  if (!reader.Open()) Fail("PmTiles Open failed", kMapPath);

  orcmap::Viewport viewport;
  viewport.center_lat_deg = kCenterLat;
  viewport.center_lon_deg = kCenterLon;
  viewport.zoom = kZoom;
  viewport.width_px = display_width;
  viewport.height_px = display_height;
  viewport.tile_size_px = kTileSizePx;

  std::vector<orcmap::TileId> tiles;
  const int64_t enumerate_started = NowUs();
  if (!orcmap::EnumerateVisibleTiles(viewport, &tiles)) {
    Fail("EnumerateVisibleTiles failed", nullptr);
  }
  const double enumerate_ms =
      static_cast<double>(NowUs() - enumerate_started) / 1000.0;

  const size_t internal_before = InternalFree();
  const size_t psram_before = PsramFree();
  ESP_LOGI(kTag,
           "mem before render internal_free=%u largest=%u psram_free=%u "
           "psram_largest=%u",
           static_cast<unsigned>(internal_before),
           static_cast<unsigned>(InternalLargest()),
           static_cast<unsigned>(psram_before),
           static_cast<unsigned>(PsramLargest()));

  orcmap::m5gfx_adapter::DisplayTarget target(g_display);
  const orcmap::MapStyle& style = LoadStyle();
  if (!orcmap::ClearMapBackground(viewport, style, &target)) {
    Fail("ClearMapBackground failed", nullptr);
  }

  double lookup_ms = 0;
  double gzip_ms = 0;
  double decode_ms = 0;
  double translate_ms = 0;
  double classify_ms = 0;
  double render_ms = 0;
  size_t present = 0;
  size_t missing = 0;
  bool sampled = false;
  const int64_t frame_started = NowUs();

  for (const orcmap::TileId& tile : tiles) {
    std::vector<uint8_t> stored;
    int64_t started = NowUs();
    const bool found = reader.GetTile(tile.z, tile.x, tile.y, &stored);
    lookup_ms += static_cast<double>(NowUs() - started) / 1000.0;
    if (!found) {
      ++missing;
      continue;
    }
    ++present;

    if (!sampled) {
      ESP_LOGI(kTag,
               "busy-sample z=%u x=%u y=%u stored=%zu after GetTile: "
               "internal=%u psram=%u",
               tile.z, tile.x, tile.y, stored.size(),
               static_cast<unsigned>(InternalFree()),
               static_cast<unsigned>(PsramFree()));
    }

    std::vector<uint8_t> raw;
    started = NowUs();
    if (!orcmap::DecompressPayload(reader.Header().tile_compression,
                                   stored.data(), stored.size(),
                                   kDecompressBudget, &raw)) {
      ESP_LOGE(kTag, "decompress failed z=%u x=%u y=%u", tile.z, tile.x,
               tile.y);
      continue;
    }
    gzip_ms += static_cast<double>(NowUs() - started) / 1000.0;
    stored.clear();
    stored.shrink_to_fit();
    if (!sampled) {
      ESP_LOGI(kTag, "after decompress raw=%zu internal=%u psram=%u", raw.size(),
               static_cast<unsigned>(InternalFree()),
               static_cast<unsigned>(PsramFree()));
    }

    orcmap::MvtTile mvt;
    started = NowUs();
    orcmap::MvtDecodeOptions decode_options;
    decode_options.include_layer =
        &orcmap::experimental::IncludeNoTextBasemapLayer;
    if (!orcmap::DecodeMvtTile(raw.data(), raw.size(), decode_options, &mvt)) {
      ESP_LOGE(kTag, "mvt decode failed z=%u x=%u y=%u", tile.z, tile.x,
               tile.y);
      continue;
    }
    decode_ms += static_cast<double>(NowUs() - started) / 1000.0;
    raw.clear();
    raw.shrink_to_fit();
    if (!sampled) {
      ESP_LOGI(kTag, "after MvtTile layers=%zu internal=%u psram=%u",
               mvt.layers.size(), static_cast<unsigned>(InternalFree()),
               static_cast<unsigned>(PsramFree()));
    }

    orcmap::FeatureTile features;
    started = NowUs();
    if (!orcmap::TranslateMvtToFeatureTile(mvt, &features)) continue;
    translate_ms += static_cast<double>(NowUs() - started) / 1000.0;
    mvt = orcmap::MvtTile{};

    started = NowUs();
    orcmap::experimental::AssignFeatureKinds(&features);
    classify_ms += static_cast<double>(NowUs() - started) / 1000.0;

    started = NowUs();
    if (!orcmap::RenderFeatureTile(features, tile, viewport, style, &target)) {
      ESP_LOGE(kTag, "render failed z=%u x=%u y=%u", tile.z, tile.x, tile.y);
    }
    render_ms += static_cast<double>(NowUs() - started) / 1000.0;
    if (!sampled) {
      ESP_LOGI(kTag, "after render internal=%u psram=%u",
               static_cast<unsigned>(InternalFree()),
               static_cast<unsigned>(PsramFree()));
      sampled = true;
    }
  }

  const double total_ms =
      static_cast<double>(NowUs() - frame_started) / 1000.0;
  const double power_to_map_ms =
      static_cast<double>(NowUs() - power_started) / 1000.0;
  ESP_LOGI(kTag, "ORCMAPS LILYGO T-DISPLAY-S3 HARDWARE DEMO");
  ESP_LOGI(kTag, "display: %dx%d", display_width, display_height);
  ESP_LOGI(kTag, "map: %s", kMapPath);
  ESP_LOGI(kTag, "zoom: %u style: %s", kZoom, style.id);
  ESP_LOGI(kTag, "visible: %zu present: %zu missing: %zu", tiles.size(),
           present, missing);
  ESP_LOGI(kTag,
           "timing enumerate_ms=%.3f GetTile=%.3f gzip=%.3f decode=%.3f "
           "translate=%.3f classify=%.3f render=%.3f total=%.3f",
           enumerate_ms, lookup_ms, gzip_ms, decode_ms, translate_ms,
           classify_ms, render_ms, total_ms);
  ESP_LOGI(kTag, "ByteSource bytes_read=%llu",
           static_cast<unsigned long long>(source.BytesRead()));
  ESP_LOGI(kTag,
           "memory internal_before=%u internal_min=%u psram_before=%u "
           "psram_min=%u psram_total=%u",
           static_cast<unsigned>(internal_before),
           static_cast<unsigned>(InternalMin()),
           static_cast<unsigned>(psram_before),
           static_cast<unsigned>(PsramMin()),
           static_cast<unsigned>(esp_psram_get_size()));
  ESP_LOGI(kTag, "RESULT: PASS");

  const orcmap_demo::Report report{
      display_width,
      display_height,
      style.id,
      tiles.size(),
      present,
      missing,
      enumerate_ms,
      lookup_ms,
      gzip_ms,
      decode_ms,
      translate_ms,
      classify_ms,
      render_ms,
      total_ms,
      power_to_map_ms,
      source.BytesRead(),
      internal_before,
      InternalMin(),
      psram_before,
      PsramMin(),
      esp_psram_get_size(),
  };
  char report_path[64];
  const bool report_saved = orcmap_demo::WriteNextReport(
      kReportDirectory, report, report_path, sizeof(report_path));
  ESP_LOGI(kTag, "report: %s %s", report_path,
           report_saved ? "SAVED" : "WRITE FAILED");
  g_display.fillRect(0, display_height - 12, display_width, 12, TFT_BLACK);
  orcmap_demo::FormatResultLine(line, sizeof(line), total_ms, report_saved);
  StatusLine(display_height - 11, report_saved ? TFT_GREEN : TFT_RED,
             line);

  for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}
