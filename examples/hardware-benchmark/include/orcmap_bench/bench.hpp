#pragma once

// Reusable, board-agnostic OrcMaps hardware benchmark harness (Layer A of
// docs/superpowers/specs/2026-09-14-tab5-sd-hardware-benchmark-demo-design.md).
//
// This component deliberately contains NO M5GFX, M5Unified, Tab5, LilyGO,
// CYD, LVGL, SDMMC, or display-controller types. It accepts an existing
// ByteSource, RenderTarget, Viewport, MapStyle, a microsecond clock, and
// memory-snapshot callbacks, then drives the real OrcMaps pipeline:
//
//   ByteSource -> PmTilesReader -> GetTile -> DecompressPayload
//              -> DecodeMvtTile -> TranslateMvtToFeatureTile
//              -> profile classification -> RenderFeatureTile -> RenderTarget
//
// There is exactly ONE pipeline implementation (RenderMeasuredFrame). The
// interactive demo calls it and ignores the statistics; benchmark scenarios
// call it and emit them. Instrumentation is supplied via hooks rather than a
// second renderer or decoder, so normal redraws and measured runs can never
// diverge.

#include <cstddef>
#include <cstdint>

#include "orcmap/feature.hpp"
#include "orcmap/pmtiles.hpp"
#include "orcmap/render_target.hpp"
#include "orcmap/style.hpp"
#include "orcmap/viewport.hpp"

namespace orcmap_bench {

// A measurement that the platform may be unable to provide. Emitted as JSON
// `null` rather than 0, so a missing probe is never mistaken for a real
// zero reading.
struct Optional64 {
  bool has_value = false;
  int64_t value = 0;

  static Optional64 Of(int64_t v) { return Optional64{true, v}; }
  static Optional64 None() { return Optional64{}; }
};

// Board-supplied probes. Any of the memory callbacks may be null; the
// corresponding schema field then emits null.
struct BenchHooks {
  int64_t (*now_us)() = nullptr;
  size_t (*internal_free)() = nullptr;
  size_t (*internal_min)() = nullptr;
  size_t (*internal_largest)() = nullptr;
  size_t (*psram_free)() = nullptr;
  size_t (*psram_min)() = nullptr;
  size_t (*psram_largest)() = nullptr;
  // Receives one complete output line (no trailing newline). The board
  // decides whether that means serial, an SD file, or both.
  void (*emit_line)(const char* line, void* ctx) = nullptr;
  void* emit_ctx = nullptr;
};

// Per-frame pipeline options. Kept here so the demo and the benchmark use
// identical decode policy; a divergence would invalidate comparisons.
struct BenchPipelineOptions {
  // Layer filter passed to DecodeMvtTile. Null decodes every layer.
  bool (*include_layer)(const char* name, size_t name_len, void* ctx) = nullptr;
  void* include_layer_ctx = nullptr;
  // Upper bound handed to DecompressPayload for a single tile payload.
  size_t decompress_budget = 512u * 1024u;
  // FeatureKind assignment, supplied by the caller so this component does
  // not depend on the EXPERIMENTAL classifier (or on whatever replaces it
  // once a real tile-content schema is chosen). The renderer skips
  // unclassified features, so a caller that wants visible geography must
  // provide this.
  void (*classify)(orcmap::FeatureTile* tile) = nullptr;
};

// One measured frame. All timings are milliseconds; all byte counts are
// bytes. Stage totals aggregate every visible present tile.
struct BenchFrame {
  bool ok = false;
  const char* error_stage = nullptr;

  size_t tiles_visible = 0;
  size_t tiles_present = 0;
  size_t tiles_missing = 0;
  size_t features_total = 0;
  uint64_t bytes_stored = 0;       // compressed bytes handed to decompress
  uint64_t bytes_decompressed = 0; // bytes produced by decompress
  Optional64 byte_source_bytes;    // ByteSource delta, if the board tracks it

  double enumerate_ms = 0.0;
  double lookup_ms = 0.0;
  double inflate_ms = 0.0;
  double decode_ms = 0.0;
  double translate_ms = 0.0;
  double classify_ms = 0.0;
  double render_ms = 0.0;
  double frame_ms = 0.0;

  Optional64 internal_free_before;
  Optional64 internal_free_after;
  Optional64 internal_min;
  Optional64 internal_largest;
  Optional64 psram_free_before;
  Optional64 psram_free_after;
  Optional64 psram_min;
  Optional64 psram_largest;
};

// A screen-independent benchmark scenario. Only viewport width/height come
// from the board, so the same scenario is comparable across displays.
//
// `pack_region_id` names the pack to measure explicitly. Benchmarks bind
// their source directly rather than going through ResolvePack(), because
// measurement and eligibility are different questions: the historical
// Springfield z14 view is wider than that extract's bounding box, so
// ResolvePack() correctly refuses it for display while it remains a valid,
// repeatable pipeline measurement. Frame records therefore always state
// which pack was measured and how many tiles were missing.
struct BenchScenario {
  const char* id = nullptr;
  const char* pack_region_id = nullptr;
  double center_lat_deg = 0.0;
  double center_lon_deg = 0.0;
  int zoom = 0;
};

// THE single measured pipeline. Renders the viewport's visible tiles from
// `reader` into `target`, filling `out` when non-null.
//
// `background` clears the map area through ClearMapBackground first; the
// interactive demo sets this false when it has already blitted a cached
// canvas underneath.
bool RenderMeasuredFrame(orcmap::PmTilesReader& reader,
                         const orcmap::Viewport& viewport,
                         const orcmap::MapStyle& style,
                         orcmap::RenderTarget* target,
                         const BenchPipelineOptions& options,
                         const BenchHooks& hooks, bool background,
                         BenchFrame* out);

// Emits one JSONL record (prefixed `ORCMAPS_BENCH_JSON `) describing a
// measured frame. `phase` is "cold" or "warm".
void EmitFrameRecord(const BenchHooks& hooks, const char* scenario_id,
                     const char* phase, const char* pack_id,
                     const char* style_id, const orcmap::Viewport& viewport,
                     const BenchFrame& frame);

// Emits a run-identity record: board/MCU/display/build strings supplied by
// the board example, so this component stays free of board headers.
struct BenchIdentity {
  const char* bench_version = nullptr;
  const char* board = nullptr;
  const char* mcu = nullptr;
  const char* idf_version = nullptr;
  const char* graphics_adapter = nullptr;
  const char* orcmaps_commit = nullptr;
  int display_width = 0;
  int display_height = 0;
  Optional64 psram_total;
  Optional64 flash_total;
};

void EmitIdentityRecord(const BenchHooks& hooks, const BenchIdentity& identity);

// Emits a storage-throughput record measured by the board (sequential read
// of a mounted file, which needs filesystem APIs this component avoids).
void EmitStorageRecord(const BenchHooks& hooks, size_t block_bytes,
                       uint64_t bytes, double elapsed_ms);

// Escapes a JSON string body into `out`. Exposed for host tests, which are
// the only place the escaping rules are actually asserted.
void JsonEscape(const char* input, char* out, size_t out_size);

}  // namespace orcmap_bench
