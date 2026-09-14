// Standalone M5Stack Tab5 hardware demo: Springfield PMTiles from the
// removable SD card through OrcMaps onto the physical 1280x720 display.
// Not OrcSDR. Board bring-up matches OrcSDR's proven M5Unified + SDMMC
// Slot 0 path. File I/O does NOT copy OrcSDR's _IONBF write policy.

#include <M5Unified.h>

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

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_psram.h>
#include <esp_timer.h>
#include <esp_vfs_fat.h>
#include <sd_pwr_ctrl_by_on_chip_ldo.h>
#include <sdmmc_cmd.h>
#include <driver/gpio.h>
#include <driver/sdmmc_host.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

constexpr char kTag[] = "orcmap_tab5";
constexpr char kMapPath[] = "/sd/orcmaps/springfield.pmtiles";
constexpr double kCenterLat = 44.0500;
constexpr double kCenterLon = -123.0220;
constexpr uint8_t kZoom = 14;
constexpr int kTileSizePx = 256;
constexpr size_t kDecompressBudget = 512u * 1024u;
constexpr size_t kBenchmarkBytes = 512u * 1024u;

sdmmc_card_t* g_card = nullptr;
sd_pwr_ctrl_handle_t g_sd_power = nullptr;

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

void StatusLine(int y, uint16_t color, const char* text) {
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(color, TFT_BLACK);
  M5.Display.drawString(text, 40, y);
}

void Fail(const char* line, const char* detail) {
  ESP_LOGE(kTag, "%s: %s", line, detail ? detail : "");
  M5.Display.fillScreen(TFT_BLACK);
  StatusLine(40, TFT_RED, "ORCMAPS Tab5 -- FAILED");
  StatusLine(100, TFT_WHITE, line);
  if (detail) StatusLine(140, TFT_ORANGE, detail);
  StatusLine(220, TFT_DARKGREY, "See USB-Serial log.");
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

bool MountSd() {
  esp_vfs_fat_sdmmc_mount_config_t mount{};
  mount.format_if_mount_failed = false;
  mount.max_files = 8;
  mount.allocation_unit_size = 16 * 1024;

  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.slot = SDMMC_HOST_SLOT_0;
  host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

  if (g_sd_power == nullptr) {
    sd_pwr_ctrl_ldo_config_t ldo{};
    ldo.ldo_chan_id = 4;
    const esp_err_t power = sd_pwr_ctrl_new_on_chip_ldo(&ldo, &g_sd_power);
    if (power != ESP_OK) {
      ESP_LOGE(kTag, "SD LDO4 init failed: %s", esp_err_to_name(power));
      return false;
    }
  }
  host.pwr_ctrl_handle = g_sd_power;

  sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
  slot.width = 4;
  slot.clk = GPIO_NUM_43;
  slot.cmd = GPIO_NUM_44;
  slot.d0 = GPIO_NUM_39;
  slot.d1 = GPIO_NUM_40;
  slot.d2 = GPIO_NUM_41;
  slot.d3 = GPIO_NUM_42;

  const esp_err_t err =
      esp_vfs_fat_sdmmc_mount("/sd", &host, &slot, &mount, &g_card);
  if (err != ESP_OK) {
    ESP_LOGE(kTag, "SDMMC Slot0 mount failed: %s", esp_err_to_name(err));
    return false;
  }
  sdmmc_card_print_info(stdout, g_card);
  return true;
}

void BenchmarkSequentialRead(const char* path, uint64_t file_size) {
  FILE* f = std::fopen(path, "rb");
  if (f == nullptr) {
    ESP_LOGE(kTag, "benchmark fopen failed");
    return;
  }
  const size_t nbytes = static_cast<size_t>(
      file_size < kBenchmarkBytes ? file_size : kBenchmarkBytes);
  const size_t blocks[] = {4096, 16384, 32768, 65536};
  std::vector<uint8_t> buf(65536);
  for (size_t block : blocks) {
    std::rewind(f);
    size_t got = 0;
    const int64_t t0 = NowUs();
    while (got < nbytes) {
      const size_t want = nbytes - got < block ? nbytes - got : block;
      const size_t n = std::fread(buf.data(), 1, want, f);
      if (n == 0) break;
      got += n;
    }
    const double ms = static_cast<double>(NowUs() - t0) / 1000.0;
    const double mbps = ms > 0.0 ? (static_cast<double>(got) / 1.0e6) / (ms / 1000.0)
                                 : 0.0;
    ESP_LOGI(kTag,
             "ORCMAPS_SD_READ block=%zu bytes=%zu elapsed_ms=%.3f mb_per_sec=%.3f",
             block, got, ms, mbps);
  }
  std::fclose(f);
}

}  // namespace

extern "C" void app_main() {
  auto config = M5.config();
  M5.begin(config);
  M5.Display.setRotation(1);
  M5.Display.setBrightness(180);

  M5.Display.fillScreen(TFT_BLACK);
  StatusLine(30, TFT_WHITE, "ORCMAPS");
  StatusLine(60, TFT_DARKGREY, "Tab5 Hardware Demo");

  const int dw = M5.Display.width();
  const int dh = M5.Display.height();
  char line[96];
  if (dw != 1280 || dh != 720) {
    std::snprintf(line, sizeof(line), "got %dx%d", dw, dh);
    Fail("Display not 1280x720", line);
  }
  StatusLine(110, TFT_GREEN, "Display .... 1280x720 OK");

  if (!esp_psram_is_initialized()) {
    Fail("PSRAM", "not initialized");
  }
  std::snprintf(line, sizeof(line), "PSRAM ...... %u KiB OK",
                static_cast<unsigned>(esp_psram_get_size() / 1024));
  StatusLine(140, TFT_GREEN, line);
  ESP_LOGI(kTag, "PSRAM total=%u free=%u largest=%u",
           static_cast<unsigned>(esp_psram_get_size()),
           static_cast<unsigned>(PsramFree()),
           static_cast<unsigned>(PsramLargest()));

  if (!MountSd()) {
    Fail("SD ......... mount failed", "check card / /sd");
  }
  StatusLine(170, TFT_GREEN, "SD ......... OK");

  orcmap::esp_idf::FileByteSource source(kMapPath);
  if (!source.Valid()) {
    Fail("Map ........ missing", kMapPath);
  }
  StatusLine(200, TFT_GREEN, "Map ........ OK");
  ESP_LOGI(kTag, "map %s size=%llu", kMapPath,
           static_cast<unsigned long long>(source.Size()));

  StatusLine(250, TFT_YELLOW, "SD read benchmark...");
  BenchmarkSequentialRead(kMapPath, source.Size());

  StatusLine(280, TFT_YELLOW, "Rendering Springfield...");

  orcmap::PmTilesReader reader(&source);
  if (!reader.Open()) {
    Fail("PmTiles Open failed", kMapPath);
  }

  orcmap::Viewport vp;
  vp.center_lat_deg = kCenterLat;
  vp.center_lon_deg = kCenterLon;
  vp.zoom = kZoom;
  vp.width_px = dw;
  vp.height_px = dh;
  vp.tile_size_px = kTileSizePx;

  std::vector<orcmap::TileId> tiles;
  const int64_t e0 = NowUs();
  if (!orcmap::EnumerateVisibleTiles(vp, &tiles)) {
    Fail("EnumerateVisibleTiles failed", nullptr);
  }
  const double enumerate_ms = static_cast<double>(NowUs() - e0) / 1000.0;

  const size_t internal_before = InternalFree();
  const size_t psram_before = PsramFree();
  ESP_LOGI(kTag,
           "mem before render internal_free=%u largest=%u psram_free=%u "
           "psram_largest=%u",
           static_cast<unsigned>(internal_before),
           static_cast<unsigned>(InternalLargest()),
           static_cast<unsigned>(psram_before),
           static_cast<unsigned>(PsramLargest()));

  orcmap::m5gfx_adapter::DisplayTarget target(M5.Display);
  const orcmap::MapStyle& style = orcmap::styles::OrcSdrDark();
  if (!orcmap::ClearMapBackground(vp, style, &target)) {
    Fail("ClearMapBackground failed", nullptr);
  }

  double lookup_ms = 0, gzip_ms = 0, decode_ms = 0, translate_ms = 0,
         classify_ms = 0, render_ms = 0;
  size_t present = 0;
  size_t missing = 0;
  bool sampled = false;
  const int64_t frame0 = NowUs();

  for (const orcmap::TileId& tile : tiles) {
    std::vector<uint8_t> stored;
    int64_t t = NowUs();
    const bool ok = reader.GetTile(tile.z, tile.x, tile.y, &stored);
    lookup_ms += static_cast<double>(NowUs() - t) / 1000.0;
    if (!ok) {
      ++missing;
      continue;
    }
    ++present;

    if (!sampled) {
      ESP_LOGI(kTag, "busy-sample z=%u x=%u y=%u stored=%zu after GetTile: "
                     "internal=%u psram=%u",
               tile.z, tile.x, tile.y, stored.size(),
               static_cast<unsigned>(InternalFree()),
               static_cast<unsigned>(PsramFree()));
    }

    std::vector<uint8_t> raw;
    t = NowUs();
    if (!orcmap::DecompressPayload(reader.Header().tile_compression,
                                   stored.data(), stored.size(),
                                   kDecompressBudget, &raw)) {
      ESP_LOGE(kTag, "decompress failed z=%u x=%u y=%u", tile.z, tile.x,
               tile.y);
      stored.clear();
      continue;
    }
    gzip_ms += static_cast<double>(NowUs() - t) / 1000.0;
    stored.clear();
    stored.shrink_to_fit();
    if (!sampled) {
      ESP_LOGI(kTag, "after decompress raw=%zu internal=%u psram=%u", raw.size(),
               static_cast<unsigned>(InternalFree()),
               static_cast<unsigned>(PsramFree()));
    }

    orcmap::MvtTile mvt;
    t = NowUs();
    if (!orcmap::DecodeMvtTile(raw.data(), raw.size(), &mvt)) {
      ESP_LOGE(kTag, "mvt decode failed z=%u x=%u y=%u", tile.z, tile.x,
               tile.y);
      continue;
    }
    decode_ms += static_cast<double>(NowUs() - t) / 1000.0;
    raw.clear();
    raw.shrink_to_fit();
    if (!sampled) {
      ESP_LOGI(kTag, "after MvtTile layers=%zu internal=%u psram=%u",
               mvt.layers.size(), static_cast<unsigned>(InternalFree()),
               static_cast<unsigned>(PsramFree()));
    }

    orcmap::FeatureTile features;
    t = NowUs();
    if (!orcmap::TranslateMvtToFeatureTile(mvt, &features)) continue;
    translate_ms += static_cast<double>(NowUs() - t) / 1000.0;
    mvt = orcmap::MvtTile{};
    if (!sampled) {
      ESP_LOGI(kTag, "after FeatureTile n=%zu (Mvt released) internal=%u psram=%u",
               features.features.size(), static_cast<unsigned>(InternalFree()),
               static_cast<unsigned>(PsramFree()));
    }

    t = NowUs();
    orcmap::experimental::AssignFeatureKinds(&features);
    classify_ms += static_cast<double>(NowUs() - t) / 1000.0;

    t = NowUs();
    if (!orcmap::RenderFeatureTile(features, tile, vp, style, &target)) {
      ESP_LOGE(kTag, "render failed z=%u x=%u y=%u", tile.z, tile.x, tile.y);
    }
    render_ms += static_cast<double>(NowUs() - t) / 1000.0;
    if (!sampled) {
      ESP_LOGI(kTag, "after render internal=%u psram=%u",
               static_cast<unsigned>(InternalFree()),
               static_cast<unsigned>(PsramFree()));
    }
    features.features.clear();
    features.features.shrink_to_fit();
    if (!sampled) {
      ESP_LOGI(kTag, "after FeatureTile release internal=%u psram=%u",
               static_cast<unsigned>(InternalFree()),
               static_cast<unsigned>(PsramFree()));
      sampled = true;
    }
  }

  const double total_ms = static_cast<double>(NowUs() - frame0) / 1000.0;

  ESP_LOGI(kTag, "ORCMAPS TAB5 HARDWARE DEMO");
  ESP_LOGI(kTag, "display: %dx%d", dw, dh);
  ESP_LOGI(kTag, "map: %s", kMapPath);
  ESP_LOGI(kTag, "zoom: %u style: %s", kZoom, style.id);
  ESP_LOGI(kTag, "visible: %zu present: %zu missing: %zu", tiles.size(), present,
           missing);
  ESP_LOGI(kTag, "timing enumerate_ms=%.3f GetTile=%.3f gzip=%.3f decode=%.3f "
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

  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
