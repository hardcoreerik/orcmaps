// Layer B: Tab5-optimized interactive OrcMaps demo.
//
// A user-facing map application first, with the reusable hardware benchmark
// harness (examples/hardware-benchmark) underneath it. See
// docs/superpowers/specs/2026-09-14-tab5-sd-hardware-benchmark-demo-design.md
//
// This file owns everything Tab5-specific: M5Unified bring-up, the 1280x720
// layout, touch interpretation, SDMMC Slot 0, ESP32-P4 heap/PSRAM probes,
// the M5GFX DisplayTarget, plain-language error screens, and report files.
// It owns no map semantics: geography, styling, camera math, and the
// measured pipeline all come from OrcMaps and the shared harness.
//
// Board bring-up matches OrcSDR's proven M5Unified + SDMMC Slot 0 path.
// Not OrcSDR; nothing here is copied from OrcSDR's write policy.

#include <M5Unified.h>

#include "orcmap/compression.hpp"
#include "orcmap/esp_idf/file_byte_source.hpp"
#include "orcmap/experimental/mvt_classify.hpp"
#include "orcmap/feature.hpp"
#include "orcmap/m5gfx/display_target.hpp"
#include "orcmap/pack.hpp"
#include "orcmap/pmtiles.hpp"
#include "orcmap/renderer.hpp"
#include "orcmap/style.hpp"
#include "orcmap/viewport.hpp"
#include "orcmap_bench/bench.hpp"

#include <driver/gpio.h>
#include <driver/sdmmc_host.h>
#include <esp_heap_caps.h>
#include <esp_idf_version.h>
#include <esp_log.h>
#include <esp_psram.h>
#include <esp_timer.h>
#include <esp_vfs_fat.h>
#include <sd_pwr_ctrl_by_on_chip_ldo.h>
#include <sdmmc_cmd.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

constexpr char kTag[] = "orcmap_tab5";
constexpr char kBenchVersion[] = "tab5-bench-1";

constexpr char kWorldPath[] = "/sd/orcmaps/world-overview.pmtiles";
constexpr char kRegionalPath[] = "/sd/orcmaps/springfield.pmtiles";
constexpr char kReportDir[] = "/sd/orcmaps";

// Chrome geometry for 1280x720. The map is the product; chrome is two thin
// bars and nothing else.
constexpr int kTopBarH = 44;
constexpr int kBottomBarH = 76;

// Touch handling. A drag must exceed this before it counts as a pan, so a
// slightly imprecise tap still activates a button.
constexpr int kDragThresholdPx = 12;

constexpr size_t kDecompressBudget = 512u * 1024u;
constexpr size_t kStorageBenchBytes = 512u * 1024u;

// Colors chosen to sit quietly against orcsdr-dark. The map style itself is
// never redefined here.
constexpr uint16_t kChromeBg = 0x0861;   // near-black graphite
constexpr uint16_t kChromeFg = 0xCE59;   // off-white
constexpr uint16_t kChromeDim = 0x6B4D;  // muted grey
constexpr uint16_t kChromeOk = 0x2E68;   // desaturated green
constexpr uint16_t kChromeWarn = 0xFCC0; // amber

sdmmc_card_t* g_card = nullptr;
sd_pwr_ctrl_handle_t g_sd_power = nullptr;

// ---------------------------------------------------------------------------
// Board probes handed to the shared harness.
// ---------------------------------------------------------------------------

int64_t NowUs() { return esp_timer_get_time(); }
size_t InternalFree() { return heap_caps_get_free_size(MALLOC_CAP_INTERNAL); }
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

// Serial is authoritative; the SD copy is best-effort and never blocks the UI.
FILE* g_report = nullptr;

void EmitLine(const char* line, void* /*ctx*/) {
  ESP_LOGI(kTag, "%s", line);
  if (g_report != nullptr) {
    std::fprintf(g_report, "%s\n", line);
    std::fflush(g_report);
  }
}

orcmap_bench::BenchHooks MakeHooks() {
  orcmap_bench::BenchHooks hooks;
  hooks.now_us = &NowUs;
  hooks.internal_free = &InternalFree;
  hooks.internal_min = &InternalMin;
  hooks.internal_largest = &InternalLargest;
  hooks.psram_free = &PsramFree;
  hooks.psram_min = &PsramMin;
  hooks.psram_largest = &PsramLargest;
  hooks.emit_line = &EmitLine;
  return hooks;
}

void ClassifyExperimental(orcmap::FeatureTile* tile) {
  orcmap::experimental::AssignFeatureKinds(tile);
}

orcmap_bench::BenchPipelineOptions MakePipelineOptions() {
  orcmap_bench::BenchPipelineOptions options;
  options.include_layer = &orcmap::experimental::IncludeNoTextBasemapLayer;
  options.decompress_budget = kDecompressBudget;
  options.classify = &ClassifyExperimental;
  return options;
}

// ---------------------------------------------------------------------------
// Boot screen / plain-language failures
// ---------------------------------------------------------------------------

void SplashLine(int y, uint16_t color, const char* text) {
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(color, TFT_BLACK);
  M5.Display.drawString(text, 60, y);
}

// A normal user sees a sentence, not an ESP-IDF code. Detail goes to serial.
[[noreturn]] void Fail(const char* headline, const char* detail) {
  ESP_LOGE(kTag, "FAILED %s: %s", headline, detail != nullptr ? detail : "");
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(kChromeWarn, TFT_BLACK);
  M5.Display.drawString("OrcMaps", 60, 80);
  M5.Display.setTextSize(4);
  M5.Display.setTextColor(kChromeFg, TFT_BLACK);
  M5.Display.drawString(headline, 60, 160);
  if (detail != nullptr) {
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(kChromeDim, TFT_BLACK);
    M5.Display.drawString(detail, 60, 240);
  }
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(kChromeDim, TFT_BLACK);
  M5.Display.drawString("Details on USB serial.", 60, 300);
  for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
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

void OpenNumberedReport() {
  for (int i = 1; i <= 9999; ++i) {
    char path[96];
    std::snprintf(path, sizeof(path), "%s/orcmaps-benchmark-%04d.jsonl",
                  kReportDir, i);
    FILE* probe = std::fopen(path, "rb");
    if (probe != nullptr) {
      std::fclose(probe);
      continue;  // Never overwrite an existing report.
    }
    g_report = std::fopen(path, "wb");
    if (g_report != nullptr) ESP_LOGI(kTag, "report file %s", path);
    return;
  }
}

// ---------------------------------------------------------------------------
// Compiled-in pack manifests.
//
// Runtime JSON manifest discovery is not implemented, so the exact verified
// sidecar values are compiled in and then validated with the real
// ValidatePackManifest() before use -- the demo does not invent metadata or
// skip validation just because the values are local.
// ---------------------------------------------------------------------------

orcmap::PackManifest MakeWorldManifest() {
  orcmap::PackManifest m;
  m.pack_id =
      "5.1.2__orcmaps-overview-1__overview__world__"
      "b-1800000000_-850511288_1800000000_850511288__z0-7";
  m.pack_version = "1";
  m.display_name = "World overview";
  m.region_id = "world";
  m.region_name = "World";
  m.bounds.min_lon_deg = -180.0;
  m.bounds.max_lon_deg = 180.0;
  // Use the engine's own Mercator limit, not the manifest JSON's rounded
  // 85.0511288: ValidBounds() requires <= kMercatorMaxLatDeg (85.05112878),
  // so the rounded value is rejected by 2e-8. llround(x * 1e7) still yields
  // +/-850511288, so pack_id identity is unchanged.
  // NOTE: build_world_overview.py writes the rounded value, which means the
  // generated manifest JSON on disk does not currently pass
  // ValidatePackManifest(). That builder rounding needs fixing before
  // runtime JSON discovery can load these packs.
  m.bounds.min_lat_deg = -orcmap::kMercatorMaxLatDeg;
  m.bounds.max_lat_deg = orcmap::kMercatorMaxLatDeg;
  m.min_zoom = 0;
  m.max_zoom = 7;
  m.content_profile = "overview";
  m.pmtiles_version = 3;
  m.schema_version = "orcmaps-overview-1";
  m.source_snapshot = "5.1.2";
  m.builder = "Planetiler / go-pmtiles";
  m.builder_version = "Planetiler 0.10.2; go-pmtiles 1.28.2";
  m.builder_commit = "0e5588c4a6e8c29a270a33afe8df62027d889604";
  m.build_date = "2026-09-14";
  m.provenance_ids = {"natural-earth"};
  m.pack_class = "clean";
  // Natural Earth requires no attribution; the courtesy credit is shown in
  // Info rather than presented as a legal requirement.
  m.size_bytes = 9737500;
  m.output_sha256 =
      "a6942c11782eb843235bbfdf78de89de0c6fea25c9a5a37abb67aab5cca4028c";
  m.priority = 0;  // Regional detail outranks the overview where eligible.
  m.archive_path = kWorldPath;
  return m;
}

orcmap::PackManifest MakeRegionalManifest() {
  orcmap::PackManifest m;
  m.pack_id =
      "geofabrik-oregon-2026-09-14__openmaptiles-3.16__standard__"
      "springfield-97477__b-1230550000_440300000_-1229600000_440900000__z0-15";
  m.pack_version = "1";
  m.display_name = "Springfield regional";
  m.region_id = "springfield-97477";
  m.region_name = "Springfield, Oregon";
  m.bounds.min_lon_deg = -123.055;
  m.bounds.min_lat_deg = 44.03;
  m.bounds.max_lon_deg = -122.96;
  m.bounds.max_lat_deg = 44.09;
  m.min_zoom = 0;
  m.max_zoom = 15;
  m.content_profile = "standard";
  m.pmtiles_version = 3;
  m.schema_version = "openmaptiles-3.16";
  m.source_snapshot = "geofabrik-oregon-2026-09-14";
  m.builder = "Planetiler / go-pmtiles";
  m.builder_version = "Planetiler 0.10.2; go-pmtiles 1.28.2";
  m.builder_commit = "0e5588c4a6e8c29a270a33afe8df62027d889604";
  m.build_date = "2026-09-14";
  m.provenance_ids = {"openstreetmap"};
  m.pack_class = "open";
  orcmap::AttributionInfo osm;
  osm.required = true;
  osm.text = "(c) OpenStreetMap contributors";
  osm.url = "https://www.openstreetmap.org/copyright";
  m.attribution.push_back(osm);
  m.size_bytes = 3507636;
  m.output_sha256 =
      "8bf23873915668f41d098b98df32b11a6d08ec6754a63d885fce2f29abe4adfd";
  m.priority = 10;
  m.archive_path = kRegionalPath;
  return m;
}

const char* ValidationErrorName(orcmap::PackValidationError error) {
  switch (error) {
    case orcmap::PackValidationError::kNone: return "none";
    case orcmap::PackValidationError::kRequiredField: return "required-field";
    case orcmap::PackValidationError::kBounds: return "bounds";
    case orcmap::PackValidationError::kZoomRange: return "zoom-range";
    case orcmap::PackValidationError::kCompatibility: return "compatibility";
    case orcmap::PackValidationError::kProvenance: return "provenance";
    case orcmap::PackValidationError::kAttribution: return "attribution";
    case orcmap::PackValidationError::kArchive: return "archive";
    case orcmap::PackValidationError::kIdentity: return "identity";
  }
  return "unknown";
}

// ---------------------------------------------------------------------------
// Map source state. ResolvePack() returns exactly one eligible pack; this
// demo therefore renders exactly one basemap at a time and never composites.
// ---------------------------------------------------------------------------

struct MapSource {
  orcmap::esp_idf::FileByteSource* bytes = nullptr;
  orcmap::PmTilesReader* reader = nullptr;
  const orcmap::PackManifest* manifest = nullptr;
};

MapSource g_world;
MapSource g_regional;
MapSource g_active;

orcmap::PackCatalog g_catalog;
orcmap::Viewport g_viewport;
const orcmap::MapStyle* g_style = nullptr;
bool g_show_info = false;
bool g_detail_unavailable = false;

// Offscreen PSRAM canvas: the whole reason interaction can feel immediate
// without a tile cache. A completed frame lives here, so a drag blits an
// offset copy instead of re-running the pipeline. UI chrome is drawn to the
// panel afterwards, outside all timed boundaries.
M5Canvas* g_canvas = nullptr;
int g_map_w = 0;
int g_map_h = 0;

int MapTop() { return kTopBarH; }

MapSource* SourceForManifest(const orcmap::PackManifest* manifest) {
  if (manifest == nullptr) return nullptr;
  if (g_regional.manifest != nullptr &&
      manifest->pack_id == g_regional.manifest->pack_id) {
    return &g_regional;
  }
  if (g_world.manifest != nullptr &&
      manifest->pack_id == g_world.manifest->pack_id) {
    return &g_world;
  }
  return nullptr;
}

// Chooses the single eligible pack for the current view. Coverage rules are
// never relaxed: an uncovered z8+ view reports missing detail rather than
// drawing a partially covered source as if it were global.
void ResolveActiveSource() {
  orcmap::GeoBounds bounds{};
  if (!orcmap::GetVisibleBounds(g_viewport, &bounds)) {
    g_detail_unavailable = true;
    return;
  }
  const orcmap::PackManifest* chosen =
      orcmap::ResolvePack(g_catalog, bounds, orcmap::GetZoom(g_viewport));
  MapSource* source = SourceForManifest(chosen);
  if (source == nullptr) {
    g_detail_unavailable = true;
    return;
  }
  g_detail_unavailable = false;
  g_active = *source;
}

uint8_t ActiveMaxZoom() {
  if (g_active.manifest != nullptr) return g_active.manifest->max_zoom;
  return 7;
}

// ---------------------------------------------------------------------------
// Rendering: the map frame goes through the shared harness pipeline, then
// chrome is drawn. Chrome is never inside the measured region.
// ---------------------------------------------------------------------------

orcmap_bench::BenchFrame g_last_frame;

void RenderMapFrame() {
  ResolveActiveSource();

  if (g_detail_unavailable || g_active.reader == nullptr) {
    g_canvas->fillScreen(kChromeBg);
    g_canvas->setTextDatum(middle_center);
    g_canvas->setTextSize(3);
    g_canvas->setTextColor(kChromeWarn, kChromeBg);
    g_canvas->drawString("DETAIL NOT INSTALLED FOR THIS VIEW", g_map_w / 2,
                         g_map_h / 2);
    g_canvas->setTextSize(2);
    g_canvas->setTextColor(kChromeDim, kChromeBg);
    g_canvas->drawString("Zoom out to the world overview", g_map_w / 2,
                         g_map_h / 2 + 48);
    return;
  }

  orcmap::m5gfx_adapter::DisplayTarget target(*g_canvas);
  orcmap::Viewport map_vp = g_viewport;
  map_vp.width_px = g_map_w;
  map_vp.height_px = g_map_h;

  const orcmap_bench::BenchHooks hooks = MakeHooks();
  const orcmap_bench::BenchPipelineOptions options = MakePipelineOptions();
  orcmap_bench::RenderMeasuredFrame(*g_active.reader, map_vp, *g_style, &target,
                                    options, hooks, /*background=*/true,
                                    &g_last_frame);
}

void DrawChrome() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();

  // Top bar: identity, active pack, zoom, SD state.
  M5.Display.fillRect(0, 0, w, kTopBarH, kChromeBg);
  M5.Display.setTextDatum(middle_left);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(kChromeFg, kChromeBg);
  M5.Display.drawString("OrcMaps", 20, kTopBarH / 2);

  char mid[96];
  const char* pack_name = g_detail_unavailable || g_active.manifest == nullptr
                              ? "No local coverage"
                              : g_active.manifest->display_name.c_str();
  std::snprintf(mid, sizeof(mid), "Z%u  %u", orcmap::GetZoom(g_viewport), 0u);
  M5.Display.setTextDatum(middle_center);
  std::snprintf(mid, sizeof(mid), "Z%-2u  %s", orcmap::GetZoom(g_viewport),
                pack_name);
  M5.Display.setTextColor(kChromeDim, kChromeBg);
  M5.Display.drawString(mid, w / 2, kTopBarH / 2);

  M5.Display.setTextDatum(middle_right);
  M5.Display.setTextColor(g_card != nullptr ? kChromeOk : kChromeWarn,
                          kChromeBg);
  M5.Display.drawString(g_card != nullptr ? "SD" : "NO SD", w - 20,
                        kTopBarH / 2);

  // Bottom bar: large touch targets plus the active source's attribution.
  const int by = h - kBottomBarH;
  M5.Display.fillRect(0, by, w, kBottomBarH, kChromeBg);
  struct Button {
    const char* label;
    int x, width;
  };
  const Button buttons[] = {
      {"World", 20, 150}, {"-", 190, 90}, {"+", 290, 90},
      {"Springfield", 400, 220}, {"Style", 640, 130}, {"Info", 790, 120},
  };
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(2);
  for (const Button& b : buttons) {
    M5.Display.drawRoundRect(b.x, by + 12, b.width, kBottomBarH - 24, 6,
                             kChromeDim);
    M5.Display.setTextColor(kChromeFg, kChromeBg);
    M5.Display.drawString(b.label, b.x + b.width / 2, by + kBottomBarH / 2);
  }

  // Attribution comes from the active manifest, never a hardcoded constant.
  const char* attribution = "";
  if (!g_detail_unavailable && g_active.manifest != nullptr) {
    for (const orcmap::AttributionInfo& info : g_active.manifest->attribution) {
      if (info.required && !info.text.empty()) {
        attribution = info.text.c_str();
        break;
      }
    }
  }
  M5.Display.setTextDatum(middle_right);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(kChromeDim, kChromeBg);
  M5.Display.drawString(attribution, w - 20, by + kBottomBarH / 2);
}

void DrawInfoPanel() {
  if (!g_show_info) return;
  const int pw = 520;
  const int ph = 300;
  const int px = 40;
  const int py = MapTop() + 40;
  M5.Display.fillRoundRect(px, py, pw, ph, 10, kChromeBg);
  M5.Display.drawRoundRect(px, py, pw, ph, 10, kChromeDim);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(2);

  char line[128];
  int y = py + 20;
  const auto put = [&](uint16_t color, const char* text) {
    M5.Display.setTextColor(color, kChromeBg);
    M5.Display.drawString(text, px + 20, y);
    y += 30;
  };
  put(kChromeFg, "OrcMaps demo");
  std::snprintf(line, sizeof(line), "Pack: %s",
                g_active.manifest != nullptr
                    ? g_active.manifest->display_name.c_str()
                    : "none");
  put(kChromeDim, line);
  const orcmap::LatLon center = orcmap::GetCenter(g_viewport);
  std::snprintf(line, sizeof(line), "Center: %.4f, %.4f", center.lat_deg,
                center.lon_deg);
  put(kChromeDim, line);
  std::snprintf(line, sizeof(line), "Zoom: %u  (max %u)",
                orcmap::GetZoom(g_viewport), ActiveMaxZoom());
  put(kChromeDim, line);
  std::snprintf(line, sizeof(line), "Style: %s", g_style->id);
  put(kChromeDim, line);
  std::snprintf(line, sizeof(line), "Frame: %.0f ms  tiles %u/%u",
                g_last_frame.frame_ms,
                static_cast<unsigned>(g_last_frame.tiles_present),
                static_cast<unsigned>(g_last_frame.tiles_visible));
  put(kChromeDim, line);
  put(kChromeDim, "Made with Natural Earth (overview)");
  put(kChromeWarn, "Tap Info again to close. Bench = run scenarios.");
}

void PresentCanvas(int offset_x, int offset_y) {
  g_canvas->pushSprite(offset_x, MapTop() + offset_y);
}

void Present() {
  PresentCanvas(0, 0);
  DrawChrome();
  DrawInfoPanel();
}

void Redraw() {
  RenderMapFrame();
  Present();
  // Requirement: serial records performance evidence while the demo is
  // simply being used, not only during the benchmark action. Emitted after
  // Present() so neither the report write nor the chrome draw is inside the
  // measured frame.
  if (!g_detail_unavailable && g_active.manifest != nullptr) {
    orcmap::Viewport map_vp = g_viewport;
    map_vp.width_px = g_map_w;
    map_vp.height_px = g_map_h;
    orcmap_bench::EmitFrameRecord(MakeHooks(), "interactive", "live",
                                  g_active.manifest->pack_id.c_str(),
                                  g_style->id, map_vp, g_last_frame);
  }
}

// ---------------------------------------------------------------------------
// Benchmark scenarios, run from Info. Same pipeline as interactive redraws.
// ---------------------------------------------------------------------------

void BenchmarkSequentialRead(const char* path, uint64_t file_size,
                             const orcmap_bench::BenchHooks& hooks) {
  FILE* f = std::fopen(path, "rb");
  if (f == nullptr) return;
  const size_t nbytes = static_cast<size_t>(
      file_size < kStorageBenchBytes ? file_size : kStorageBenchBytes);
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
    orcmap_bench::EmitStorageRecord(hooks, block, got, ms);
  }
  std::fclose(f);
}

void RunScenario(const orcmap_bench::BenchScenario& scenario,
                 const orcmap_bench::BenchHooks& hooks) {
  orcmap::Viewport saved = g_viewport;
  const MapSource saved_active = g_active;

  orcmap::SetCenter(&g_viewport, scenario.center_lat_deg,
                    scenario.center_lon_deg);
  orcmap::SetZoom(&g_viewport, scenario.zoom);

  // Bind the scenario's pack directly. Measurement is not the same question
  // as display eligibility: the historical Springfield z14 viewport is wider
  // than that extract, so ResolvePack() rightly refuses it for the demo while
  // it stays a valid repeatable measurement. tiles_missing records the
  // partial coverage instead of hiding it.
  MapSource* source = nullptr;
  if (scenario.pack_region_id != nullptr) {
    if (g_regional.manifest != nullptr &&
        g_regional.manifest->region_id == scenario.pack_region_id) {
      source = &g_regional;
    } else if (g_world.manifest != nullptr &&
               g_world.manifest->region_id == scenario.pack_region_id) {
      source = &g_world;
    }
  }
  if (source == nullptr || source->reader == nullptr) {
    ESP_LOGW(kTag, "scenario %s: pack %s not installed", scenario.id,
             scenario.pack_region_id != nullptr ? scenario.pack_region_id : "?");
    g_viewport = saved;
    g_active = saved_active;
    return;
  }
  g_active = *source;

  orcmap::Viewport map_vp = g_viewport;
  map_vp.width_px = g_map_w;
  map_vp.height_px = g_map_h;
  orcmap::m5gfx_adapter::DisplayTarget target(*g_canvas);
  const orcmap_bench::BenchPipelineOptions options = MakePipelineOptions();

  const uint64_t bytes_before = g_active.bytes->BytesRead();
  orcmap_bench::BenchFrame cold;
  orcmap_bench::RenderMeasuredFrame(*g_active.reader, map_vp, *g_style, &target,
                                    options, hooks, true, &cold);
  cold.byte_source_bytes = orcmap_bench::Optional64::Of(
      static_cast<int64_t>(g_active.bytes->BytesRead() - bytes_before));
  orcmap_bench::EmitFrameRecord(hooks, scenario.id, "cold",
                                g_active.manifest->pack_id.c_str(), g_style->id,
                                map_vp, cold);

  const uint64_t warm_before = g_active.bytes->BytesRead();
  orcmap_bench::BenchFrame warm;
  orcmap_bench::RenderMeasuredFrame(*g_active.reader, map_vp, *g_style, &target,
                                    options, hooks, true, &warm);
  warm.byte_source_bytes = orcmap_bench::Optional64::Of(
      static_cast<int64_t>(g_active.bytes->BytesRead() - warm_before));
  orcmap_bench::EmitFrameRecord(hooks, scenario.id, "warm",
                                g_active.manifest->pack_id.c_str(), g_style->id,
                                map_vp, warm);

  g_viewport = saved;
  g_active = saved_active;
}

void RunBenchmarks() {
  const orcmap_bench::BenchHooks hooks = MakeHooks();

  M5.Display.fillRect(0, MapTop(), M5.Display.width(), g_map_h, TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(kChromeFg, TFT_BLACK);
  M5.Display.drawString("Running benchmarks...", M5.Display.width() / 2,
                        MapTop() + g_map_h / 2);

  BenchmarkSequentialRead(kRegionalPath, g_regional.bytes->Size(), hooks);

  // The original Springfield z14 proof stays a repeatable scenario so past
  // hardware numbers remain comparable.
  const orcmap_bench::BenchScenario scenarios[] = {
      {"world", "world", 0.0, 0.0, 0},
      {"oregon", "world", 46.0, -121.0, 4},
      // Historical hardware proof, kept byte-for-byte comparable with the
      // earlier ~4.4 s / 11.3 s Tab5 numbers. At z14 this viewport is wider
      // than the extract, so tiles_missing is expected to be non-zero.
      {"springfield", "springfield-97477", 44.0500, -123.0220, 14},
      // The zoom where the extract actually covers a full 1280x600 view,
      // which is what the interactive demo uses.
      {"springfield-covered", "springfield-97477", 44.0500, -123.0220, 15},
  };
  for (const orcmap_bench::BenchScenario& scenario : scenarios) {
    RunScenario(scenario, hooks);
  }

  Redraw();
}

// ---------------------------------------------------------------------------
// Touch: interpreted here, applied through generic OrcMaps camera APIs only.
// ---------------------------------------------------------------------------

bool HitButton(int x, int y, int bx, int bw) {
  const int by = M5.Display.height() - kBottomBarH;
  return x >= bx && x < bx + bw && y >= by;
}

void HandleButtonTap(int x, int y) {
  if (HitButton(x, y, 20, 150)) {  // World
    orcmap::SetZoom(&g_viewport, 0);
    orcmap::SetCenter(&g_viewport, 20.0, 0.0);
    Redraw();
  } else if (HitButton(x, y, 190, 90)) {  // -
    orcmap::ZoomOut(&g_viewport);
    Redraw();
  } else if (HitButton(x, y, 290, 90)) {  // +
    // Overzoom is rejected by the viewport, so stop at the active pack's
    // max zoom instead of producing an empty frame.
    if (orcmap::GetZoom(g_viewport) < ActiveMaxZoom()) {
      orcmap::ZoomIn(&g_viewport);
      Redraw();
    }
  } else if (HitButton(x, y, 400, 220)) {  // Springfield
    // z15, not z14: ResolvePack() requires full coverage and the extract is
    // narrower than a 1280x600 z14 viewport. z14 stays available as a
    // benchmark scenario, where partial coverage is measured not displayed.
    orcmap::SetCenter(&g_viewport, 44.0500, -123.0220);
    orcmap::SetZoom(&g_viewport, 15);
    Redraw();
  } else if (HitButton(x, y, 640, 130)) {  // Style
    g_style = (g_style == &orcmap::styles::OrcSdrDark())
                  ? &orcmap::styles::StandardLight()
                  : &orcmap::styles::OrcSdrDark();
    Redraw();
  } else if (HitButton(x, y, 790, 120)) {  // Info
    g_show_info = !g_show_info;
    Present();
  } else if (g_show_info && HitButton(x, y, 920, 200)) {
    RunBenchmarks();
  }
}

}  // namespace

extern "C" void app_main() {
  auto config = M5.config();
  M5.begin(config);
  M5.Display.setRotation(1);
  M5.Display.setBrightness(180);
  M5.Display.fillScreen(TFT_BLACK);

  SplashLine(80, kChromeFg, "OrcMaps");
  SplashLine(120, kChromeDim, "Offline embedded maps");

  const int dw = M5.Display.width();
  const int dh = M5.Display.height();
  char line[128];
  if (dw != 1280 || dh != 720) {
    std::snprintf(line, sizeof(line), "got %dx%d", dw, dh);
    Fail("UNEXPECTED DISPLAY SIZE", line);
  }
  SplashLine(200, kChromeOk, "Display     1280x720 OK");

  if (!esp_psram_is_initialized()) Fail("PSRAM NOT AVAILABLE", nullptr);
  std::snprintf(line, sizeof(line), "PSRAM       %u KiB OK",
                static_cast<unsigned>(esp_psram_get_size() / 1024));
  SplashLine(240, kChromeOk, line);

  if (!MountSd()) Fail("SD CARD NOT FOUND", "insert a card with /orcmaps");
  SplashLine(280, kChromeOk, "SD card     OK");

  // Pack manifests: compiled in, but still validated for real.
  orcmap::PackManifest world = MakeWorldManifest();
  orcmap::PackManifest regional = MakeRegionalManifest();
  for (const orcmap::PackManifest* m : {&world, &regional}) {
    const orcmap::PackValidationError err = orcmap::ValidatePackManifest(*m);
    if (err != orcmap::PackValidationError::kNone) {
      ESP_LOGE(kTag, "manifest %s invalid: %s", m->pack_id.c_str(),
               ValidationErrorName(err));
      Fail("MAP PACK METADATA INVALID", ValidationErrorName(err));
    }
  }

  // Either pack alone is enough to show a usable map. Whatever the user
  // actually copied to the card, they get geography rather than a dead end.
  static orcmap::esp_idf::FileByteSource world_bytes(kWorldPath);
  static orcmap::PmTilesReader world_reader(&world_bytes);
  bool have_world = world_bytes.Valid() && world_reader.Open();
  SplashLine(320, have_world ? kChromeOk : kChromeWarn,
             have_world ? "World pack  OK"
                        : "World pack  not installed");

  static orcmap::esp_idf::FileByteSource regional_bytes(kRegionalPath);
  static orcmap::PmTilesReader regional_reader(&regional_bytes);
  bool have_regional = regional_bytes.Valid() && regional_reader.Open();
  SplashLine(360, have_regional ? kChromeOk : kChromeWarn,
             have_regional ? "Region pack OK"
                           : "Region pack not installed");

  if (!have_world && !have_regional) {
    Fail("NO MAP PACKS FOUND",
         "copy world-overview.pmtiles to /orcmaps on the SD card");
  }
  if (have_world && !g_catalog.Add(world)) {
    ESP_LOGW(kTag, "world pack rejected by catalog");
    have_world = false;
  }
  if (have_regional && !g_catalog.Add(regional)) {
    ESP_LOGW(kTag, "regional pack rejected by catalog");
    have_regional = false;
  }
  if (g_catalog.Packs().empty()) Fail("MAP PACK METADATA REJECTED", nullptr);

  // Bind readers to the catalog's stored manifests so pointers stay valid.
  for (const orcmap::PackManifest& m : g_catalog.Packs()) {
    if (m.archive_path == kWorldPath) {
      g_world.bytes = &world_bytes;
      g_world.reader = &world_reader;
      g_world.manifest = &m;
    } else if (m.archive_path == kRegionalPath) {
      g_regional.bytes = &regional_bytes;
      g_regional.reader = &regional_reader;
      g_regional.manifest = &m;
    }
  }

  g_map_w = dw;
  g_map_h = dh - kTopBarH - kBottomBarH;
  g_canvas = new M5Canvas(&M5.Display);
  g_canvas->setPsram(true);
  g_canvas->setColorDepth(16);
  if (!g_canvas->createSprite(g_map_w, g_map_h)) {
    Fail("NOT ENOUGH MEMORY FOR MAP", "offscreen canvas allocation failed");
  }
  SplashLine(400, kChromeOk, "Map buffer  OK");
  SplashLine(450, kChromeFg, "OrcMaps ready");

  OpenNumberedReport();

  orcmap_bench::BenchIdentity identity;
  identity.bench_version = kBenchVersion;
  identity.board = "M5Stack Tab5";
  identity.mcu = "ESP32-P4";
  identity.idf_version = IDF_VER;
  identity.graphics_adapter = "M5GFX DisplayTarget";
  identity.display_width = dw;
  identity.display_height = dh;
  identity.psram_total =
      orcmap_bench::Optional64::Of(static_cast<int64_t>(esp_psram_get_size()));
  orcmap_bench::EmitIdentityRecord(MakeHooks(), identity);

  // Start on a real map, never a benchmark menu.
  g_style = &orcmap::styles::OrcSdrDark();
  g_viewport = orcmap::Viewport{};
  g_viewport.width_px = g_map_w;
  g_viewport.height_px = g_map_h;
  g_viewport.tile_size_px = 256;
  // Start on whichever installed pack can actually show something: the
  // world overview when present, otherwise the regional pack's own area.
  if (have_world) {
    orcmap::SetCenter(&g_viewport, 20.0, 0.0);
    orcmap::SetZoom(&g_viewport, 0);
  } else {
    orcmap::SetCenter(&g_viewport, 44.0500, -123.0220);
    orcmap::SetZoom(&g_viewport, 15);
  }

  vTaskDelay(pdMS_TO_TICKS(600));
  M5.Display.fillScreen(TFT_BLACK);
  Redraw();

  // Interactive loop. A drag blits the finished canvas at an offset for
  // immediate feedback; the pipeline re-runs once on release.
  bool dragging = false;
  int press_x = 0, press_y = 0;
  int last_dx = 0, last_dy = 0;

  for (;;) {
    M5.update();
    const auto touch = M5.Touch.getDetail();

    if (touch.isPressed()) {
      if (!dragging) {
        press_x = touch.x;
        press_y = touch.y;
        dragging = true;
        last_dx = last_dy = 0;
      } else if (press_y >= MapTop() &&
                 press_y < M5.Display.height() - kBottomBarH) {
        const int dx = touch.x - press_x;
        const int dy = touch.y - press_y;
        if (std::abs(dx) > kDragThresholdPx || std::abs(dy) > kDragThresholdPx) {
          if (dx != last_dx || dy != last_dy) {
            // Presentation-only: no decode, no SD, no pipeline.
            M5.Display.fillRect(0, MapTop(), M5.Display.width(), g_map_h,
                                TFT_BLACK);
            PresentCanvas(dx, dy);
            last_dx = dx;
            last_dy = dy;
          }
        }
      }
    } else if (dragging) {
      dragging = false;
      const int dx = last_dx;
      const int dy = last_dy;
      if (std::abs(dx) > kDragThresholdPx || std::abs(dy) > kDragThresholdPx) {
        // Gesture moves the map with the finger, so the camera moves the
        // opposite way.
        orcmap::PanByPixels(&g_viewport, -dx, -dy);
        Redraw();
      } else {
        HandleButtonTap(press_x, press_y);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(16));
  }
}
