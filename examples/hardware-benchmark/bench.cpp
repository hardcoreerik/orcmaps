#include "orcmap_bench/bench.hpp"

#include <new>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <vector>

#include "orcmap/compression.hpp"
#include "orcmap/mvt.hpp"
#include "orcmap/mvt_translate.hpp"
#include "orcmap/renderer.hpp"

namespace orcmap_bench {

namespace {

constexpr size_t kLineMax = 2048;

int64_t Now(const BenchHooks& hooks) {
  return hooks.now_us != nullptr ? hooks.now_us() : 0;
}

double MsSince(const BenchHooks& hooks, int64_t start_us) {
  if (hooks.now_us == nullptr) return 0.0;
  return static_cast<double>(hooks.now_us() - start_us) / 1000.0;
}

Optional64 Probe(size_t (*fn)()) {
  if (fn == nullptr) return Optional64::None();
  return Optional64::Of(static_cast<int64_t>(fn()));
}

// Minimal bounded appender. Every emit path uses this so a long pack id or
// style name truncates instead of overflowing.
struct LineBuffer {
  char data[kLineMax];
  size_t len = 0;

  void Add(const char* fmt, ...) __attribute__((format(printf, 2, 3)));
  void AddOptional(const char* key, const Optional64& value);
  void AddString(const char* key, const char* value);
};

void LineBuffer::Add(const char* fmt, ...) {
  if (len >= sizeof(data) - 1) return;
  va_list args;
  va_start(args, fmt);
  const int n = std::vsnprintf(data + len, sizeof(data) - len, fmt, args);
  va_end(args);
  if (n > 0) {
    len += static_cast<size_t>(n);
    if (len > sizeof(data) - 1) len = sizeof(data) - 1;
  }
}

void LineBuffer::AddOptional(const char* key, const Optional64& value) {
  if (value.has_value) {
    Add("\"%s\":%lld,", key, static_cast<long long>(value.value));
  } else {
    Add("\"%s\":null,", key);
  }
}

void LineBuffer::AddString(const char* key, const char* value) {
  if (value == nullptr) {
    Add("\"%s\":null,", key);
    return;
  }
  char escaped[256];
  JsonEscape(value, escaped, sizeof(escaped));
  Add("\"%s\":\"%s\",", key, escaped);
}

// Removes the trailing comma (if any) and closes the object, then emits.
void Finish(const BenchHooks& hooks, LineBuffer* line) {
  if (line->len > 0 && line->data[line->len - 1] == ',') --line->len;
  if (line->len < sizeof(line->data) - 2) {
    line->data[line->len++] = '}';
  }
  line->data[line->len] = '\0';
  if (hooks.emit_line != nullptr) hooks.emit_line(line->data, hooks.emit_ctx);
}

void Begin(LineBuffer* line, const char* record) {
  line->len = 0;
  line->Add("ORCMAPS_BENCH_JSON {\"record\":\"%s\",", record);
}

}  // namespace

void JsonEscape(const char* input, char* out, size_t out_size) {
  if (out == nullptr || out_size == 0) return;
  size_t o = 0;
  if (input == nullptr) {
    out[0] = '\0';
    return;
  }
  for (size_t i = 0; input[i] != '\0'; ++i) {
    const unsigned char c = static_cast<unsigned char>(input[i]);
    const char* escape = nullptr;
    char buf[7];
    switch (c) {
      case '"': escape = "\\\""; break;
      case '\\': escape = "\\\\"; break;
      case '\n': escape = "\\n"; break;
      case '\r': escape = "\\r"; break;
      case '\t': escape = "\\t"; break;
      default:
        if (c < 0x20) {
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          escape = buf;
        }
        break;
    }
    if (escape != nullptr) {
      const size_t n = std::strlen(escape);
      if (o + n >= out_size) break;
      std::memcpy(out + o, escape, n);
      o += n;
    } else {
      if (o + 1 >= out_size) break;
      out[o++] = static_cast<char>(c);
    }
  }
  out[o] = '\0';
}

bool RenderMeasuredFrame(orcmap::PmTilesReader& reader,
                         const orcmap::Viewport& viewport,
                         const orcmap::MapStyle& style,
                         orcmap::RenderTarget* target,
                         const BenchPipelineOptions& options,
                         const BenchHooks& hooks, bool background,
                         BenchFrame* out) {
  BenchFrame frame;
  frame.internal_free_before = Probe(hooks.internal_free);
  frame.psram_free_before = Probe(hooks.psram_free);

  const int64_t frame_start = Now(hooks);

  // Placements, not plain tiles: a viewport wider than the world draws the
  // same tile at several longitudes so the map extends around itself. Group
  // them so each distinct tile is fetched and decoded ONCE and only the
  // render step repeats -- otherwise wrapping would multiply the expensive
  // stages and make the measurement meaningless.
  std::vector<orcmap::TilePlacement> placements;
  const int64_t enum_start = Now(hooks);
  if (!orcmap::EnumerateVisibleTilePlacements(viewport, &placements)) {
    frame.error_stage = "enumerate";
    if (out != nullptr) *out = frame;
    return false;
  }

  std::vector<orcmap::TileId> tiles;
  std::vector<std::vector<int64_t>> copies;
  for (const orcmap::TilePlacement& p : placements) {
    size_t index = tiles.size();
    for (size_t i = 0; i < tiles.size(); ++i) {
      if (tiles[i].z == p.tile.z && tiles[i].x == p.tile.x &&
          tiles[i].y == p.tile.y) {
        index = i;
        break;
      }
    }
    if (index == tiles.size()) {
      tiles.push_back(p.tile);
      copies.emplace_back();
    }
    copies[index].push_back(p.unwrapped_x);
  }
  frame.enumerate_ms = MsSince(hooks, enum_start);
  // tiles_visible stays the count of DISTINCT tiles, so it remains
  // comparable with runs recorded before world-copy placement existed.
  frame.tiles_visible = tiles.size();
  frame.placements = placements.size();

  if (background && !orcmap::ClearMapBackground(viewport, style, target)) {
    frame.error_stage = "background";
    if (out != nullptr) *out = frame;
    return false;
  }

  for (size_t ti = 0; ti < tiles.size(); ++ti) {
    const orcmap::TileId& tile = tiles[ti];
    // Heap floor check before any per-tile allocation.
    if (options.min_free_internal_bytes > 0 && hooks.internal_free != nullptr &&
        hooks.internal_free() < options.min_free_internal_bytes) {
      ++frame.tiles_skipped_memory;
      continue;
    }
    // On a board without PSRAM a dense tile can exhaust the heap mid-decode,
    // and with C++ exceptions disabled a failed allocation calls abort() --
    // taking a multi-minute benchmark run with it. Where the board enables
    // exceptions, that becomes a recorded skip instead of a crash. The
    // pre-tile heap floor above cannot catch this on its own: it samples
    // free memory BEFORE the tile, while the decoder's vectors grow during
    // it.
    // Declared outside the try so the handler can undo a count made before
    // the allocation failed: a tile fetched successfully but abandoned
    // mid-decode must not be reported as BOTH present and skipped.
    bool counted_present = false;
#if defined(__cpp_exceptions) && __cpp_exceptions
    try {
#endif
    std::vector<uint8_t> stored;
    int64_t t = Now(hooks);
    const bool got = reader.GetTile(tile.z, tile.x, tile.y, &stored);
    frame.lookup_ms += MsSince(hooks, t);
    if (!got) {
      ++frame.tiles_missing;
      continue;
    }
    ++frame.tiles_present;
    counted_present = true;
    frame.bytes_stored += stored.size();

    std::vector<uint8_t> raw;
    t = Now(hooks);
    const bool inflated = orcmap::DecompressPayload(
        reader.Header().tile_compression, stored.data(), stored.size(),
        options.decompress_budget, &raw);
    frame.inflate_ms += MsSince(hooks, t);
    stored.clear();
    stored.shrink_to_fit();
    if (!inflated) {
      frame.error_stage = "inflate";
      continue;
    }
    frame.bytes_decompressed += raw.size();

    orcmap::MvtTile mvt;
    t = Now(hooks);
    orcmap::MvtDecodeOptions decode_options;
    decode_options.include_layer = options.include_layer;
    decode_options.include_layer_ctx = options.include_layer_ctx;
    const bool decoded =
        orcmap::DecodeMvtTile(raw.data(), raw.size(), decode_options, &mvt);
    frame.decode_ms += MsSince(hooks, t);
    raw.clear();
    raw.shrink_to_fit();
    if (!decoded) {
      frame.error_stage = "decode";
      continue;
    }

    orcmap::FeatureTile features;
    t = Now(hooks);
    const bool translated = orcmap::TranslateMvtToFeatureTile(mvt, &features);
    frame.translate_ms += MsSince(hooks, t);
    mvt = orcmap::MvtTile{};
    if (!translated) {
      frame.error_stage = "translate";
      continue;
    }
    frame.features_total += features.features.size();

    if (options.classify != nullptr) {
      t = Now(hooks);
      options.classify(&features);
      frame.classify_ms += MsSince(hooks, t);
    }

    t = Now(hooks);
    for (const int64_t unwrapped_x : copies[ti]) {
      orcmap::TilePlacement placement;
      placement.tile = tile;
      placement.unwrapped_x = unwrapped_x;
      if (!orcmap::RenderFeatureTileAt(features, placement, viewport, style,
                                       target)) {
        frame.error_stage = "render";
      }
    }
    frame.render_ms += MsSince(hooks, t);

    features.features.clear();
    features.features.shrink_to_fit();
#if defined(__cpp_exceptions) && __cpp_exceptions
    } catch (const std::bad_alloc&) {
      if (counted_present && frame.tiles_present > 0) --frame.tiles_present;
      ++frame.tiles_skipped_memory;
      frame.error_stage = "out-of-memory";
      continue;
    }
#endif
  }

  frame.frame_ms = MsSince(hooks, frame_start);
  frame.internal_free_after = Probe(hooks.internal_free);
  frame.psram_free_after = Probe(hooks.psram_free);
  frame.internal_min = Probe(hooks.internal_min);
  frame.internal_largest = Probe(hooks.internal_largest);
  frame.psram_min = Probe(hooks.psram_min);
  frame.psram_largest = Probe(hooks.psram_largest);
  frame.ok = frame.error_stage == nullptr;

  if (out != nullptr) *out = frame;
  return frame.ok;
}

void EmitFrameRecord(const BenchHooks& hooks, const char* scenario_id,
                     const char* phase, const char* pack_id,
                     const char* style_id, const orcmap::Viewport& viewport,
                     const BenchFrame& frame) {
  LineBuffer line;
  Begin(&line, "frame");
  line.AddString("scenario", scenario_id);
  line.AddString("phase", phase);
  line.AddString("pack_id", pack_id);
  line.AddString("style", style_id);
  line.Add("\"zoom\":%u,", static_cast<unsigned>(viewport.zoom));
  line.Add("\"center_lat\":%.6f,", viewport.center_lat_deg);
  line.Add("\"center_lon\":%.6f,", viewport.center_lon_deg);
  line.Add("\"viewport_w\":%d,", viewport.width_px);
  line.Add("\"viewport_h\":%d,", viewport.height_px);

  line.Add("\"tiles_visible\":%u,", static_cast<unsigned>(frame.tiles_visible));
  line.Add("\"placements\":%u,", static_cast<unsigned>(frame.placements));
  line.Add("\"tiles_present\":%u,", static_cast<unsigned>(frame.tiles_present));
  line.Add("\"tiles_missing\":%u,", static_cast<unsigned>(frame.tiles_missing));
  line.Add("\"tiles_skipped_memory\":%u,",
           static_cast<unsigned>(frame.tiles_skipped_memory));
  line.Add("\"features\":%u,", static_cast<unsigned>(frame.features_total));
  line.Add("\"bytes_stored\":%llu,",
           static_cast<unsigned long long>(frame.bytes_stored));
  line.Add("\"bytes_decompressed\":%llu,",
           static_cast<unsigned long long>(frame.bytes_decompressed));
  line.AddOptional("byte_source_bytes", frame.byte_source_bytes);

  line.Add("\"enumerate_ms\":%.3f,", frame.enumerate_ms);
  line.Add("\"lookup_ms\":%.3f,", frame.lookup_ms);
  line.Add("\"inflate_ms\":%.3f,", frame.inflate_ms);
  line.Add("\"decode_ms\":%.3f,", frame.decode_ms);
  line.Add("\"translate_ms\":%.3f,", frame.translate_ms);
  line.Add("\"classify_ms\":%.3f,", frame.classify_ms);
  line.Add("\"render_ms\":%.3f,", frame.render_ms);
  line.Add("\"frame_ms\":%.3f,", frame.frame_ms);

  line.AddOptional("internal_free_before", frame.internal_free_before);
  line.AddOptional("internal_free_after", frame.internal_free_after);
  line.AddOptional("internal_min", frame.internal_min);
  line.AddOptional("internal_largest", frame.internal_largest);
  line.AddOptional("psram_free_before", frame.psram_free_before);
  line.AddOptional("psram_free_after", frame.psram_free_after);
  line.AddOptional("psram_min", frame.psram_min);
  line.AddOptional("psram_largest", frame.psram_largest);

  line.AddString("error_stage", frame.error_stage);
  line.Add("\"result\":\"%s\"", frame.ok ? "PASS" : "FAIL");
  Finish(hooks, &line);
}

void EmitIdentityRecord(const BenchHooks& hooks,
                        const BenchIdentity& identity) {
  LineBuffer line;
  Begin(&line, "identity");
  line.AddString("bench_version", identity.bench_version);
  line.AddString("board", identity.board);
  line.AddString("mcu", identity.mcu);
  line.AddString("idf_version", identity.idf_version);
  line.AddString("graphics_adapter", identity.graphics_adapter);
  line.AddString("orcmaps_commit", identity.orcmaps_commit);
  line.Add("\"display_w\":%d,", identity.display_width);
  line.Add("\"display_h\":%d,", identity.display_height);
  line.AddOptional("psram_total", identity.psram_total);
  line.AddOptional("flash_total", identity.flash_total);
  Finish(hooks, &line);
}

void SweepAccumulator::Add(const BenchFrame& frame) {
  if (frames == 0 || frame.frame_ms < min_ms) min_ms = frame.frame_ms;
  if (frames == 0 || frame.frame_ms > max_ms) max_ms = frame.frame_ms;
  ++frames;
  total_ms += frame.frame_ms;
  tiles_visible += frame.tiles_visible;
  placements += frame.placements;
  tiles_present += frame.tiles_present;
  tiles_missing += frame.tiles_missing;
  tiles_skipped_memory += frame.tiles_skipped_memory;
  features_total += frame.features_total;
  bytes_stored += frame.bytes_stored;
  bytes_decompressed += frame.bytes_decompressed;
  lookup_ms += frame.lookup_ms;
  inflate_ms += frame.inflate_ms;
  decode_ms += frame.decode_ms;
  translate_ms += frame.translate_ms;
  classify_ms += frame.classify_ms;
  render_ms += frame.render_ms;
}

double SweepAccumulator::MeanMs() const {
  return frames == 0 ? 0.0 : total_ms / static_cast<double>(frames);
}

double SweepAccumulator::Fps() const {
  const double mean = MeanMs();
  return mean > 0.0 ? 1000.0 / mean : 0.0;
}

namespace {

// Shared body of the two sweep records: counts, rate, and stage MEANS.
// Means, not totals, so a level with a different frame count stays
// comparable with its neighbours.
void AddSweepBody(LineBuffer* line, const SweepAccumulator& a) {
  const double n = a.frames == 0 ? 1.0 : static_cast<double>(a.frames);
  line->Add("\"frames\":%u,", static_cast<unsigned>(a.frames));
  line->Add("\"mean_frame_ms\":%.3f,", a.MeanMs());
  line->Add("\"min_frame_ms\":%.3f,", a.min_ms);
  line->Add("\"max_frame_ms\":%.3f,", a.max_ms);
  line->Add("\"fps\":%.3f,", a.Fps());
  line->Add("\"tiles_visible\":%u,", static_cast<unsigned>(a.tiles_visible));
  line->Add("\"placements\":%u,", static_cast<unsigned>(a.placements));
  line->Add("\"tiles_present\":%u,", static_cast<unsigned>(a.tiles_present));
  line->Add("\"tiles_missing\":%u,", static_cast<unsigned>(a.tiles_missing));
  line->Add("\"tiles_skipped_memory\":%u,",
            static_cast<unsigned>(a.tiles_skipped_memory));
  line->Add("\"features\":%u,", static_cast<unsigned>(a.features_total));
  line->Add("\"bytes_stored\":%llu,",
            static_cast<unsigned long long>(a.bytes_stored));
  line->Add("\"bytes_decompressed\":%llu,",
            static_cast<unsigned long long>(a.bytes_decompressed));
  line->Add("\"mean_lookup_ms\":%.3f,", a.lookup_ms / n);
  line->Add("\"mean_inflate_ms\":%.3f,", a.inflate_ms / n);
  line->Add("\"mean_decode_ms\":%.3f,", a.decode_ms / n);
  line->Add("\"mean_translate_ms\":%.3f,", a.translate_ms / n);
  line->Add("\"mean_classify_ms\":%.3f,", a.classify_ms / n);
  line->Add("\"mean_render_ms\":%.3f", a.render_ms / n);
}

}  // namespace

void EmitSweepZoomRecord(const BenchHooks& hooks, const char* sweep_id,
                         const char* direction, uint8_t zoom,
                         int screens_panned, int no_coverage_frames,
                         const char* pack_id,
                         const orcmap::Viewport& viewport,
                         const SweepAccumulator& accumulated) {
  LineBuffer line;
  Begin(&line, "sweep_zoom");
  line.AddString("sweep", sweep_id);
  line.AddString("direction", direction);
  line.AddString("pack_id", pack_id);
  line.Add("\"zoom\":%u,", static_cast<unsigned>(zoom));
  line.Add("\"screens_panned\":%d,", screens_panned);
  line.Add("\"no_coverage_frames\":%d,", no_coverage_frames);
  line.Add("\"viewport_w\":%d,", viewport.width_px);
  line.Add("\"viewport_h\":%d,", viewport.height_px);
  AddSweepBody(&line, accumulated);
  Finish(hooks, &line);
}

void EmitSweepSummaryRecord(const BenchHooks& hooks, const char* sweep_id,
                            uint8_t min_zoom, uint8_t max_zoom,
                            double wall_ms, int no_coverage_frames,
                            const SweepAccumulator& accumulated) {
  LineBuffer line;
  Begin(&line, "sweep_summary");
  line.AddString("sweep", sweep_id);
  line.Add("\"min_zoom\":%u,", static_cast<unsigned>(min_zoom));
  line.Add("\"max_zoom\":%u,", static_cast<unsigned>(max_zoom));
  line.Add("\"wall_ms\":%.3f,", wall_ms);
  line.Add("\"no_coverage_frames\":%d,", no_coverage_frames);
  AddSweepBody(&line, accumulated);
  Finish(hooks, &line);
}

void EmitRejectedPackRecord(const BenchHooks& hooks,
                            const orcmap::RejectedPack& rejected) {
  LineBuffer line;
  Begin(&line, "pack_rejected");
  char escaped[160];
  JsonEscape(rejected.manifest_name.c_str(), escaped, sizeof(escaped));
  line.Add("\"manifest\":\"%s\",", escaped);
  line.Add("\"reason\":\"%s\",", orcmap::PackRejectionName(rejected.rejection));
  line.Add("\"json_error\":\"%s\",",
           orcmap::PackJsonErrorName(rejected.json_error));
  line.Add("\"validation_error\":%d,",
           static_cast<int>(rejected.validation_error));
  JsonEscape(rejected.detail.c_str(), escaped, sizeof(escaped));
  line.Add("\"detail\":\"%s\"", escaped);
  Finish(hooks, &line);
}

void EmitStorageRecord(const BenchHooks& hooks, size_t block_bytes,
                       uint64_t bytes, double elapsed_ms) {
  LineBuffer line;
  Begin(&line, "storage");
  line.Add("\"block_bytes\":%u,", static_cast<unsigned>(block_bytes));
  line.Add("\"bytes\":%llu,", static_cast<unsigned long long>(bytes));
  line.Add("\"elapsed_ms\":%.3f,", elapsed_ms);
  const double mbps =
      elapsed_ms > 0.0
          ? (static_cast<double>(bytes) / 1.0e6) / (elapsed_ms / 1000.0)
          : 0.0;
  line.Add("\"mb_per_sec\":%.3f", mbps);
  Finish(hooks, &line);
}

}  // namespace orcmap_bench
