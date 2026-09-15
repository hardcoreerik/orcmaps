// OrcMaps benchmark instrument for the "Cheap Yellow Display" 3.5"
// (ESP32-3248S035: ESP32-D0WD-V3, ST7796 320x480, microSD on SPI).
//
// This is a BENCHMARK INSTRUMENT, not a port of the Tab5 demo. The board has
// no PSRAM -- roughly 300 KB of DRAM total, while a single 320x480x16bpp
// framebuffer is 307,200 bytes -- so the Tab5's offscreen-canvas drag model
// cannot exist here and is deliberately not attempted. Rendering goes
// straight to the panel, which means render timings on this board include
// SPI transfer; that is the real cost of drawing on a CYD.
//
// Everything measured comes from the shared, board-agnostic harness
// (examples/hardware-benchmark), and packs are found by the same runtime
// discovery the Tab5 uses. This file owns only board specifics: the ST7796
// bring-up, the SPI SD mount, heap probes, and the report file.
//
// Pin mapping is the configuration already proven on this hardware in the
// NEONDRIVE firmware (include/variants/cyd35/cyd35_hw_config.h), not a
// guess: display on HSPI, SD on a separate VSPI bus, so the two never
// contend.

#include <LovyanGFX.hpp>

#include "orcmap/compression.hpp"
#include "orcmap/esp_idf/file_byte_source.hpp"
#include "orcmap/esp_idf/pack_filesystem.hpp"
#include "orcmap/experimental/mvt_classify.hpp"
#include "orcmap/feature.hpp"
#include "orcmap/m5gfx/display_target.hpp"
#include "orcmap/pack.hpp"
#include "orcmap/pack_discovery.hpp"
#include "orcmap/pmtiles.hpp"
#include "orcmap/renderer.hpp"
#include "orcmap/style.hpp"
#include "orcmap/viewport.hpp"
#include "orcmap_bench/bench.hpp"

#include <driver/gpio.h>
#include <driver/sdspi_host.h>
#include <driver/spi_common.h>
#include <esp_heap_caps.h>
#include <esp_idf_version.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>

#include <cstdio>
#include <cstring>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

constexpr char kTag[] = "orcmap_cyd35";
constexpr char kBenchVersion[] = "cyd35-bench-1";

// The board owns the path; OrcMaps core never names an SD mount.
constexpr char kPackDir[] = "/sd/orcmaps";
constexpr char kReportDir[] = "/sd/orcmaps";

// Verified CYD 3.5" pin map (NEONDRIVE cyd35_hw_config.h). Display is on
// HSPI/SPI2, SD on VSPI/SPI3 -- separate buses.
constexpr int kTftMiso = 12;
constexpr int kTftMosi = 13;
constexpr int kTftSclk = 14;
constexpr int kTftCs = 15;
constexpr int kTftDc = 2;
constexpr int kTftRst = -1;  // tied high on this board
constexpr int kBacklight0 = 21;
// Rev A drives a second backlight line; Rev B leaves it unused. Driving it
// high is harmless on Rev B, and leaving it low on Rev A gives a dark panel.
constexpr int kBacklight1 = 27;

constexpr int kSdSclk = 18;
constexpr int kSdMiso = 19;
constexpr int kSdMosi = 23;
constexpr int kSdCs = 5;

// Explicit SPI DMA channel assignment; see the note in the bus config.
constexpr int kDisplayDmaChannel = 1;
constexpr int kSdDmaChannel = 2;

// LANDSCAPE. The panel is 320x480 portrait natively; rotation 1 presents it
// as 480x320, which is how this board is actually held -- text drawn in the
// native portrait orientation reads sideways on the bench.
constexpr int kDisplayRotation = 1;
constexpr int kDisplayW = 480;
constexpr int kDisplayH = 320;
// Native panel geometry, before rotation. LovyanGFX wants the panel's own
// dimensions here; rotation is applied on top.
constexpr int kPanelW = 320;
constexpr int kPanelH = 480;

// Memory policy for a board with ~300 KB of DRAM and no PSRAM.
//
// The budget is an upper bound per tile payload, not a reservation; measured
// OpenMapTiles tiles decompress to roughly 4-40 KB each. 192 KB is generous
// for that while still being refused long before it could exhaust the heap.
constexpr size_t kDecompressBudget = 192u * 1024u;
// Below this much free internal memory the harness skips a tile instead of
// attempting an allocation that would abort the firmware (exceptions are
// disabled, so a failed allocation terminates rather than throws). A skipped
// tile is recorded; a crash would take the whole run with it.
constexpr size_t kHeapFloor = 48u * 1024u;

constexpr int kSweepScreens = 5;

// ---------------------------------------------------------------------------
// Board bring-up
// ---------------------------------------------------------------------------

class Cyd35Display : public lgfx::LGFX_Device {
 public:
  Cyd35Display() {
    {
      auto config = bus_.config();
      config.spi_host = SPI2_HOST;  // HSPI
      config.spi_mode = 0;
      config.freq_write = 24000000;
      config.freq_read = 16000000;
      config.spi_3wire = false;
      config.use_lock = true;
      // The classic ESP32 has exactly TWO SPI DMA channels, and this board
      // needs both: one for the display bus, one for the SD bus. Requesting
      // SPI_DMA_CH_AUTO here consumed the allocator's choice and left the
      // SD mount failing with ESP_ERR_NOT_FOUND ("no available dma
      // channel"), so the channels are assigned explicitly instead.
      config.dma_channel = kDisplayDmaChannel;
      config.pin_sclk = kTftSclk;
      config.pin_mosi = kTftMosi;
      config.pin_miso = kTftMiso;
      config.pin_dc = kTftDc;
      bus_.config(config);
      panel_.setBus(&bus_);
    }
    {
      auto config = panel_.config();
      config.pin_cs = kTftCs;
      config.pin_rst = kTftRst;
      config.pin_busy = -1;
      config.panel_width = kPanelW;
      config.panel_height = kPanelH;
      config.offset_x = 0;
      config.offset_y = 0;
      config.offset_rotation = 0;
      config.readable = true;
      config.invert = false;
      // BGR panel: NEONDRIVE sets TFT_RGB_ORDER to TFT_BGR, which is
      // LovyanGFX's rgb_order = false.
      config.rgb_order = false;
      config.dlen_16bit = false;
      // The XPT2046 touch controller shares this SPI bus (unused here, but
      // the panel must still release the bus between transactions).
      config.bus_shared = true;
      panel_.config(config);
    }
    {
      auto config = light_.config();
      config.pin_bl = kBacklight0;
      config.invert = false;
      config.freq = 22000;
      config.pwm_channel = 7;
      light_.config(config);
      panel_.setLight(&light_);
    }
    setPanel(&panel_);
  }

 private:
  lgfx::Bus_SPI bus_;
  lgfx::Panel_ST7796 panel_;
  lgfx::Light_PWM light_;
};

Cyd35Display g_display;
sdmmc_card_t* g_card = nullptr;

int64_t NowUs() { return esp_timer_get_time(); }
size_t InternalFree() { return heap_caps_get_free_size(MALLOC_CAP_INTERNAL); }
size_t InternalMin() {
  return heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
}
size_t InternalLargest() {
  return heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
}

// Serial is authoritative; the SD copy is best-effort and never blocks.
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
  // No PSRAM on this board: these probes stay null so the schema emits JSON
  // null rather than a misleading zero.
  hooks.psram_free = nullptr;
  hooks.psram_min = nullptr;
  hooks.psram_largest = nullptr;
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
  options.min_free_internal_bytes = kHeapFloor;
  return options;
}

// ---------------------------------------------------------------------------
// Plain-language status, since this board has no room for a UI
// ---------------------------------------------------------------------------

void StatusLine(int y, uint16_t color, const char* text) {
  g_display.setTextDatum(top_left);
  g_display.setTextSize(1);
  g_display.setTextColor(color, TFT_BLACK);
  g_display.fillRect(0, y, kDisplayW, 12, TFT_BLACK);
  g_display.drawString(text, 6, y);
}

[[noreturn]] void Fail(const char* headline, const char* detail) {
  ESP_LOGE(kTag, "FAILED %s: %s", headline, detail != nullptr ? detail : "");
  g_display.fillScreen(TFT_BLACK);
  g_display.setTextDatum(top_left);
  g_display.setTextSize(2);
  g_display.setTextColor(TFT_ORANGE, TFT_BLACK);
  g_display.drawString("OrcMaps", 8, 20);
  g_display.setTextSize(1);
  g_display.setTextColor(TFT_WHITE, TFT_BLACK);
  g_display.drawString(headline, 8, 60);
  if (detail != nullptr) {
    g_display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    g_display.drawString(detail, 8, 80);
  }
  g_display.drawString("Details on USB serial.", 8, 110);
  for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}

bool MountSd() {
  spi_bus_config_t bus{};
  bus.mosi_io_num = kSdMosi;
  bus.miso_io_num = kSdMiso;
  bus.sclk_io_num = kSdSclk;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  bus.max_transfer_sz = 4000;
  const esp_err_t bus_err =
      spi_bus_initialize(SPI3_HOST, &bus,
                         static_cast<spi_dma_chan_t>(kSdDmaChannel));
  if (bus_err != ESP_OK) {
    ESP_LOGE(kTag, "SD SPI bus init failed: %s", esp_err_to_name(bus_err));
    return false;
  }

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = SPI3_HOST;

  sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot.gpio_cs = static_cast<gpio_num_t>(kSdCs);
  slot.host_id = SPI3_HOST;

  esp_vfs_fat_sdmmc_mount_config_t mount{};
  mount.format_if_mount_failed = false;
  mount.max_files = 6;
  mount.allocation_unit_size = 16 * 1024;

  const esp_err_t err =
      esp_vfs_fat_sdspi_mount("/sd", &host, &slot, &mount, &g_card);
  if (err != ESP_OK) {
    ESP_LOGE(kTag, "SD SPI mount failed: %s", esp_err_to_name(err));
    return false;
  }
  sdmmc_card_print_info(stdout, g_card);
  return true;
}

void OpenNumberedReport() {
  // Distinct prefix from the Tab5's: each board has its own card, and a
  // report name should say which device wrote it.
  for (int i = 1; i <= 9999; ++i) {
    char path[96];
    std::snprintf(path, sizeof(path), "%s/orcmaps-cyd35-%04d.jsonl", kReportDir,
                  i);
    FILE* probe = std::fopen(path, "rb");
    if (probe != nullptr) {
      std::fclose(probe);
      continue;
    }
    g_report = std::fopen(path, "wb");
    if (g_report != nullptr) ESP_LOGI(kTag, "report file %s", path);
    return;
  }
  ESP_LOGW(kTag, "no free report slot; serial only");
}

// ---------------------------------------------------------------------------
// Map state. One reader per discovered pack; no canvas, no compiled-in packs.
// ---------------------------------------------------------------------------

struct MapSource {
  orcmap::esp_idf::FileByteSource* bytes = nullptr;
  orcmap::PmTilesReader* reader = nullptr;
  const orcmap::PackManifest* manifest = nullptr;
};

std::vector<MapSource> g_sources;
MapSource g_active;
orcmap::PackCatalog g_catalog;
orcmap::Viewport g_viewport;
const orcmap::MapStyle* g_style = nullptr;
orcmap_bench::BenchFrame g_last_frame;
bool g_no_coverage = false;
uint8_t g_zoom_floor = 0;

MapSource* SourceForManifest(const orcmap::PackManifest* manifest) {
  if (manifest == nullptr) return nullptr;
  for (MapSource& source : g_sources) {
    if (source.manifest != nullptr &&
        source.manifest->pack_id == manifest->pack_id) {
      return &source;
    }
  }
  return nullptr;
}

MapSource* WidestSource() {
  MapSource* best = nullptr;
  double best_span = -1.0;
  for (MapSource& source : g_sources) {
    if (source.manifest == nullptr) continue;
    const double span = source.manifest->bounds.max_lon_deg -
                        source.manifest->bounds.min_lon_deg;
    if (span > best_span) {
      best_span = span;
      best = &source;
    }
  }
  return best;
}

MapSource* DeepestSource() {
  MapSource* best = nullptr;
  for (MapSource& source : g_sources) {
    if (source.manifest == nullptr) continue;
    if (best == nullptr ||
        source.manifest->max_zoom > best->manifest->max_zoom) {
      best = &source;
    }
  }
  return best;
}

bool BoundsOverlap(const orcmap::GeoBounds& a, const orcmap::GeoBounds& b) {
  return a.min_lon_deg <= b.max_lon_deg && a.max_lon_deg >= b.min_lon_deg &&
         a.min_lat_deg <= b.max_lat_deg && a.max_lat_deg >= b.min_lat_deg;
}

// Same two-tier rule as the Tab5: ResolvePack() decides automatic selection
// with strict full coverage, and a merely overlapping pack is still drawn
// rather than refused.
void ResolveActiveSource() {
  g_no_coverage = false;
  orcmap::GeoBounds visible{};
  if (!orcmap::GetVisibleBounds(g_viewport, &visible)) {
    g_no_coverage = true;
    return;
  }
  const uint8_t zoom = orcmap::GetZoom(g_viewport);
  if (MapSource* covered =
          SourceForManifest(orcmap::ResolvePack(g_catalog, visible, zoom))) {
    g_active = *covered;
    return;
  }
  const orcmap::PackManifest* best = nullptr;
  for (const orcmap::PackManifest& m : g_catalog.Packs()) {
    if (zoom < m.min_zoom || zoom > m.max_zoom) continue;
    if (!BoundsOverlap(m.bounds, visible)) continue;
    if (best == nullptr || m.priority > best->priority) best = &m;
  }
  MapSource* partial = SourceForManifest(best);
  if (partial == nullptr) {
    g_no_coverage = true;
    return;
  }
  g_active = *partial;
}

// The whole-world view for THIS display, derived by the engine. 320x480 at
// 256 px tiles resolves to z1 (a 512 px world covers 480 px of height),
// where the Tab5's 1280x600 resolves to z2 -- the same pack, a different
// answer, with no per-board constant.
void RecomputeZoomFloor() {
  orcmap::Viewport probe = g_viewport;
  probe.width_px = kDisplayW;
  probe.height_px = kDisplayH;
  bool have = false;
  uint8_t floor_zoom = 0;
  for (const orcmap::PackManifest& m : g_catalog.Packs()) {
    uint8_t fill = 0;
    if (!orcmap::MinFillZoom(probe, m.bounds, &fill)) continue;
    if (fill > m.max_zoom) continue;
    if (fill < m.min_zoom) fill = m.min_zoom;
    if (!have || fill < floor_zoom) {
      floor_zoom = fill;
      have = true;
    }
  }
  g_zoom_floor = have ? floor_zoom : 0;
}

bool FocusPack(const orcmap::PackManifest& manifest) {
  if (manifest.bounds.max_lon_deg - manifest.bounds.min_lon_deg >= 360.0) {
    uint8_t zoom = 0;
    orcmap::Viewport probe = g_viewport;
    probe.width_px = kDisplayW;
    probe.height_px = kDisplayH;
    if (orcmap::WorldViewZoom(probe, &zoom)) {
      if (zoom < manifest.min_zoom) zoom = manifest.min_zoom;
      if (zoom > manifest.max_zoom) zoom = manifest.max_zoom;
      if (orcmap::SetZoom(&g_viewport, zoom)) {
        return orcmap::SetCenter(&g_viewport, 0.0, 0.0);
      }
    }
  }
  if (orcmap::FillBounds(&g_viewport, manifest.bounds)) return true;
  return orcmap::FitBounds(&g_viewport, manifest.bounds, 8);
}

// One measured frame, drawn straight to the panel. There is no offscreen
// canvas on this board, so render_ms includes SPI transfer to the display --
// stated plainly rather than hidden, because it is the real cost here.
void RenderFrame() {
  ResolveActiveSource();
  if (g_no_coverage || g_active.reader == nullptr) {
    g_display.fillScreen(TFT_BLACK);
    g_display.setTextDatum(middle_center);
    g_display.setTextSize(1);
    g_display.setTextColor(TFT_ORANGE, TFT_BLACK);
    g_display.drawString("NO COVERAGE AT THIS VIEW", kDisplayW / 2,
                         kDisplayH / 2);
    // No pipeline ran, so there is no measurement to report.
    g_last_frame = orcmap_bench::BenchFrame{};
    return;
  }
  orcmap::m5gfx_adapter::DisplayTarget target(g_display);
  orcmap::Viewport map_vp = g_viewport;
  map_vp.width_px = kDisplayW;
  map_vp.height_px = kDisplayH;
  orcmap_bench::RenderMeasuredFrame(*g_active.reader, map_vp, *g_style, &target,
                                    MakePipelineOptions(), MakeHooks(),
                                    /*background=*/true, &g_last_frame);
}

// ---------------------------------------------------------------------------
// Benchmarks
// ---------------------------------------------------------------------------

void SweepAtZoom(const orcmap_bench::BenchHooks& hooks, const char* direction,
                 uint8_t zoom, orcmap_bench::SweepAccumulator* overall,
                 int* overall_blank) {
  if (!orcmap::SetZoom(&g_viewport, zoom)) return;
  if (MapSource* deepest = DeepestSource()) {
    const orcmap::GeoBounds& b = deepest->manifest->bounds;
    orcmap::SetCenter(&g_viewport, (b.min_lat_deg + b.max_lat_deg) * 0.5,
                      (b.min_lon_deg + b.max_lon_deg) * 0.5);
  }

  orcmap_bench::SweepAccumulator level;
  int blank = 0;
  const auto measure = [&]() {
    RenderFrame();
    // A frame with no coverage drew a message, not a map: not a pipeline
    // measurement, so it is counted separately instead of flattering the
    // rate.
    if (g_no_coverage || g_last_frame.frame_ms <= 0.0) {
      ++blank;
      if (overall_blank != nullptr) ++(*overall_blank);
      return;
    }
    level.Add(g_last_frame);
    if (overall != nullptr) overall->Add(g_last_frame);
  };

  measure();
  for (int i = 0; i < kSweepScreens; ++i) {
    orcmap::PanByPixels(&g_viewport, kDisplayW, 0);
    measure();
  }
  for (int i = 0; i < kSweepScreens; ++i) {
    orcmap::PanByPixels(&g_viewport, -kDisplayW, 0);
    measure();
  }

  orcmap::Viewport map_vp = g_viewport;
  map_vp.width_px = kDisplayW;
  map_vp.height_px = kDisplayH;
  orcmap_bench::EmitSweepZoomRecord(
      hooks, "zoom-pan-1", direction, zoom, kSweepScreens * 2, blank,
      g_active.manifest != nullptr ? g_active.manifest->pack_id.c_str() : "none",
      map_vp, level);

  char note[80];
  std::snprintf(note, sizeof(note), "z%u %s %.0fms %.2ffps %db",
                static_cast<unsigned>(zoom), direction, level.MeanMs(),
                level.Fps(), blank);
  StatusLine(0, TFT_WHITE, note);
}

void RunZoomPanSweep() {
  const orcmap_bench::BenchHooks hooks = MakeHooks();
  uint8_t deepest_zoom = g_zoom_floor;
  if (MapSource* deepest = DeepestSource()) {
    deepest_zoom = deepest->manifest->max_zoom;
  }
  if (deepest_zoom < g_zoom_floor) deepest_zoom = g_zoom_floor;

  orcmap_bench::SweepAccumulator overall;
  int blank = 0;
  const int64_t start = NowUs();
  for (int z = deepest_zoom; z >= static_cast<int>(g_zoom_floor); --z) {
    SweepAtZoom(hooks, "down", static_cast<uint8_t>(z), &overall, &blank);
  }
  for (int z = g_zoom_floor; z <= static_cast<int>(deepest_zoom); ++z) {
    SweepAtZoom(hooks, "up", static_cast<uint8_t>(z), &overall, &blank);
  }
  const double wall_ms = static_cast<double>(NowUs() - start) / 1000.0;
  orcmap_bench::EmitSweepSummaryRecord(hooks, "zoom-pan-1", g_zoom_floor,
                                       deepest_zoom, wall_ms, blank, overall);
}

void RunScenarios() {
  const orcmap_bench::BenchHooks hooks = MakeHooks();
  // Cold then warm at each installed pack's own framing, so the numbers are
  // about this board rather than about a coordinate chosen for another one.
  for (MapSource& source : g_sources) {
    if (source.manifest == nullptr || source.reader == nullptr) continue;
    if (!FocusPack(*source.manifest)) continue;
    g_active = source;

    orcmap::Viewport map_vp = g_viewport;
    map_vp.width_px = kDisplayW;
    map_vp.height_px = kDisplayH;
    orcmap::m5gfx_adapter::DisplayTarget target(g_display);

    orcmap_bench::BenchFrame cold;
    orcmap_bench::RenderMeasuredFrame(*source.reader, map_vp, *g_style, &target,
                                      MakePipelineOptions(), hooks, true, &cold);
    orcmap_bench::EmitFrameRecord(hooks, source.manifest->region_id.c_str(),
                                  "cold", source.manifest->pack_id.c_str(),
                                  g_style->id, map_vp, cold);

    orcmap_bench::BenchFrame warm;
    orcmap_bench::RenderMeasuredFrame(*source.reader, map_vp, *g_style, &target,
                                      MakePipelineOptions(), hooks, true, &warm);
    orcmap_bench::EmitFrameRecord(hooks, source.manifest->region_id.c_str(),
                                  "warm", source.manifest->pack_id.c_str(),
                                  g_style->id, map_vp, warm);
  }
}

}  // namespace

extern "C" void app_main() {
  g_display.init();
  g_display.setRotation(kDisplayRotation);
  g_display.setBrightness(200);
  g_display.fillScreen(TFT_BLACK);
  // Rev A wires a second backlight enable; drive it high so the panel is lit
  // on both board revisions.
  if (kBacklight1 >= 0) {
    gpio_config_t bl{};
    bl.pin_bit_mask = 1ULL << kBacklight1;
    bl.mode = GPIO_MODE_OUTPUT;
    gpio_config(&bl);
    gpio_set_level(static_cast<gpio_num_t>(kBacklight1), 1);
  }

  g_display.setTextDatum(top_left);
  g_display.setTextSize(2);
  g_display.setTextColor(TFT_WHITE, TFT_BLACK);
  g_display.drawString("OrcMaps", 8, 10);
  g_display.setTextSize(1);
  g_display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  g_display.drawString("CYD 3.5\" benchmark", 8, 36);

  char line[96];
  const int w = g_display.width();
  const int h = g_display.height();
  std::snprintf(line, sizeof(line), "Display  %dx%d", w, h);
  StatusLine(56, TFT_GREEN, line);
  if (w != kDisplayW || h != kDisplayH) {
    std::snprintf(line, sizeof(line), "expected %dx%d", kDisplayW, kDisplayH);
    Fail("UNEXPECTED DISPLAY SIZE", line);
  }

  std::snprintf(line, sizeof(line), "Heap     %u KiB free (no PSRAM)",
                static_cast<unsigned>(InternalFree() / 1024));
  StatusLine(70, TFT_GREEN, line);

  if (!MountSd()) {
    Fail("SD CARD NOT FOUND", "insert a card with /orcmaps");
  }
  StatusLine(84, TFT_GREEN, "SD card  mounted");

  OpenNumberedReport();

  static orcmap::esp_idf::PackFileSystem pack_fs;
  orcmap::DiscoveryReport discovery;
  orcmap::DiscoverPacks(pack_fs, kPackDir, &g_catalog, &discovery);
  if (!discovery.directory_listed) {
    Fail("NO /orcmaps ON CARD", "create /orcmaps and copy a pack triplet");
  }
  ESP_LOGI(kTag, "discovery: %u manifest(s), %u installed, %u rejected",
           static_cast<unsigned>(discovery.manifests_seen),
           static_cast<unsigned>(discovery.packs_added),
           static_cast<unsigned>(discovery.rejected.size()));
  for (const orcmap::RejectedPack& bad : discovery.rejected) {
    ESP_LOGW(kTag, "rejected %s: %s", bad.manifest_name.c_str(),
             orcmap::PackRejectionName(bad.rejection));
    orcmap_bench::EmitRejectedPackRecord(MakeHooks(), bad);
  }
  std::snprintf(line, sizeof(line), "Packs    %u installed, %u rejected",
                static_cast<unsigned>(discovery.packs_added),
                static_cast<unsigned>(discovery.rejected.size()));
  StatusLine(98, discovery.packs_added > 0 ? TFT_GREEN : TFT_ORANGE, line);
  if (g_catalog.Packs().empty()) {
    Fail("NO USABLE PACKS IN /orcmaps", "run tools/pack-verify on the card");
  }

  for (const orcmap::PackManifest& manifest : g_catalog.Packs()) {
    auto* bytes =
        new orcmap::esp_idf::FileByteSource(manifest.archive_path.c_str());
    auto* reader = new orcmap::PmTilesReader(bytes);
    if (!bytes->Valid() || !reader->Open()) {
      ESP_LOGW(kTag, "pack %s: archive not a readable PMTiles",
               manifest.pack_id.c_str());
      continue;
    }
    MapSource source;
    source.bytes = bytes;
    source.reader = reader;
    source.manifest = &manifest;
    g_sources.push_back(source);
    ESP_LOGI(kTag, "installed %s z%u-%u priority %d",
             manifest.region_id.c_str(),
             static_cast<unsigned>(manifest.min_zoom),
             static_cast<unsigned>(manifest.max_zoom), manifest.priority);
  }
  if (g_sources.empty()) Fail("MAP ARCHIVES UNREADABLE", "re-copy the packs");

  orcmap_bench::BenchIdentity identity;
  identity.bench_version = kBenchVersion;
  identity.board = "CYD 3.5in (ESP32-3248S035)";
  identity.mcu = "ESP32-D0WD-V3";
  identity.idf_version = esp_get_idf_version();
  identity.graphics_adapter = "M5GFX DisplayTarget (ST7796, direct to panel)";
  identity.display_width = w;
  identity.display_height = h;
  identity.psram_total = orcmap_bench::Optional64::None();  // none fitted
  identity.flash_total = orcmap_bench::Optional64::None();
  orcmap_bench::EmitIdentityRecord(MakeHooks(), identity);

  g_style = &orcmap::styles::OrcSdrDark();
  g_viewport = orcmap::Viewport{};
  g_viewport.width_px = kDisplayW;
  g_viewport.height_px = kDisplayH;
  g_viewport.tile_size_px = 256;
  RecomputeZoomFloor();

  if (MapSource* initial = WidestSource()) {
    FocusPack(*initial->manifest);
  }
  vTaskDelay(pdMS_TO_TICKS(800));
  RenderFrame();

  ESP_LOGI(kTag, "running fixed scenarios");
  RunScenarios();

#ifdef ORCMAP_CYD_AUTOSWEEP
  ESP_LOGW(kTag, "AUTOSWEEP build: running zoom/pan sweep");
  RunZoomPanSweep();
  ESP_LOGW(kTag, "AUTOSWEEP complete");
#endif

  ESP_LOGI(kTag, "benchmark done; idling");
  for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}
