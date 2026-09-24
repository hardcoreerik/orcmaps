// Host check for a firmware-embedded world pack.
//
//   orcmap_embedded_world_check ARCHIVE [PARTITION_BYTES] [--ppm-dir DIR]
//
// Loads ARCHIVE into a buffer the size of the flash partition it will live
// in, padded with 0xFF like erased flash, and reads it through a ByteSource
// that behaves like esp_idf::PartitionByteSource: Size() is the partition,
// not the archive. It then opens the archive with PmTilesReader, frames it
// with LocationPicker at three screen sizes, and streams every visible tile
// of the opening view, a picked location at the maximum zoom, and one drag.
//
// PASS means every visible tile was present and decoded. It checks the
// pipeline and the data together; it is not a hardware or timing test.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "orcmap/byte_range.hpp"
#include "orcmap/byte_source.hpp"
#include "orcmap/experimental/mvt_classify.hpp"
#include "orcmap/location_picker.hpp"
#include "orcmap/mvt_stream.hpp"
#include "orcmap/pmtiles.hpp"
#include "orcmap/renderer.hpp"
#include "orcmap/style.hpp"
#include "orcmap/viewport.hpp"
#include "framebuffer_target.hpp"

namespace {

class FlashImageSource : public orcmap::ByteSource {
 public:
  FlashImageSource(std::vector<uint8_t> image, size_t partition_bytes)
      : bytes_(std::move(image)) {
    bytes_.resize(partition_bytes, 0xFF);
  }
  size_t Read(uint64_t offset, void* destination, size_t length) override {
    size_t readable = 0;
    if (destination == nullptr ||
        !orcmap::ClampReadRange(offset, length, bytes_.size(), &readable)) {
      return 0;
    }
    std::memcpy(destination, bytes_.data() + offset, readable);
    return readable;
  }
  uint64_t Size() const override { return bytes_.size(); }
  bool Valid() const override { return true; }

 private:
  std::vector<uint8_t> bytes_;
};

struct SinkContext {
  const orcmap::Viewport* viewport = nullptr;
  orcmap::RenderTarget* target = nullptr;
  orcmap::TileId tile{};
  const std::vector<int64_t>* copies = nullptr;
  size_t features = 0;
  size_t classified = 0;
};

bool DrawFeature(const orcmap::Feature& decoded, void* ctx) {
  SinkContext& s = *static_cast<SinkContext*>(ctx);
  orcmap::Feature& feature = const_cast<orcmap::Feature&>(decoded);
  orcmap::FeatureKind kind = orcmap::FeatureKind::kBackground;
  if (orcmap::experimental::TryClassifyFeature(feature, &kind)) {
    feature.kind = kind;
    feature.kind_assigned = true;
    ++s.classified;
  }
  ++s.features;
  for (const int64_t unwrapped_x : *s.copies) {
    orcmap::TilePlacement placement;
    placement.tile = s.tile;
    placement.unwrapped_x = unwrapped_x;
    orcmap::RenderFeatureAt(feature, placement, *s.viewport,
                            orcmap::styles::StandardLight(), s.target);
  }
  return true;
}

// Streams every distinct visible tile once and draws it at each placement.
bool CheckFrame(const orcmap::PmTilesReader& reader,
                const orcmap::Viewport& viewport,
                orcmap::MvtStreamScratch* scratch, const std::string& ppm) {
  orcmap::host::FramebufferTarget framebuffer(viewport.width_px,
                                              viewport.height_px);
  orcmap::ClearMapBackground(viewport, orcmap::styles::StandardLight(),
                             &framebuffer);
  std::vector<orcmap::TilePlacement> placements;
  if (!orcmap::EnumerateVisibleTilePlacements(viewport, &placements)) {
    std::printf("  FAIL: could not enumerate tiles\n");
    return false;
  }
  std::vector<orcmap::TileId> tiles;
  std::vector<std::vector<int64_t>> copies;
  for (const orcmap::TilePlacement& p : placements) {
    size_t i = 0;
    while (i < tiles.size() && tiles[i] != p.tile) ++i;
    if (i == tiles.size()) {
      tiles.push_back(p.tile);
      copies.emplace_back();
    }
    copies[i].push_back(p.unwrapped_x);
  }
  size_t found = 0, missing = 0, failed = 0, features = 0, classified = 0;
  for (size_t i = 0; i < tiles.size(); ++i) {
    const orcmap::TileId& t = tiles[i];
    if (!reader.TileExists(t.z, t.x, t.y)) {
      ++missing;
      continue;
    }
    SinkContext ctx;
    ctx.viewport = &viewport;
    ctx.target = &framebuffer;
    ctx.tile = t;
    ctx.copies = &copies[i];
    orcmap::MvtStreamOptions options;
    options.include_layer = &orcmap::experimental::IncludeNoTextBasemapLayer;
    options.feature_sink = &DrawFeature;
    options.feature_sink_ctx = &ctx;
    if (reader.StreamTile(t.z, t.x, t.y, options, scratch)) {
      ++found;
    } else {
      ++failed;
    }
    features += ctx.features;
    classified += ctx.classified;
  }
  std::printf("  z%u at %.4f,%.4f: %zu tiles (%zu placements), found %zu, "
              "missing %zu, failed %zu, features %zu, classified %zu\n",
              viewport.zoom, viewport.center_lat_deg, viewport.center_lon_deg,
              tiles.size(), placements.size(), found, missing, failed,
              features, classified);
  if (!ppm.empty()) framebuffer.WritePpm(ppm);
  return found > 0 && missing == 0 && failed == 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr,
                 "usage: %s ARCHIVE [PARTITION_BYTES] [--ppm-dir DIR]\n",
                 argv[0]);
    return 2;
  }
  size_t partition_bytes = 0;
  std::string ppm_dir;
  for (int i = 2; i < argc; ++i) {
    if (std::strcmp(argv[i], "--ppm-dir") == 0 && i + 1 < argc) {
      ppm_dir = argv[++i];
    } else {
      partition_bytes = static_cast<size_t>(std::strtoull(argv[i], nullptr, 0));
    }
  }

  std::ifstream in(argv[1], std::ios::binary);
  std::vector<uint8_t> image((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
  if (image.empty()) {
    std::printf("FAIL: cannot read %s\n", argv[1]);
    return 1;
  }
  if (partition_bytes == 0) {
    // Round up to the next 64 KiB, the granularity a partition table uses.
    partition_bytes = (image.size() + 0xFFFF) & ~static_cast<size_t>(0xFFFF);
  }
  if (partition_bytes < image.size()) {
    std::printf("FAIL: archive (%zu bytes) does not fit a %zu-byte partition\n",
                image.size(), partition_bytes);
    return 1;
  }
  std::printf("archive %zu bytes in a %zu-byte partition (0xFF padded)\n",
              image.size(), partition_bytes);

  FlashImageSource source(std::move(image), partition_bytes);
  orcmap::PmTilesReader reader(&source, 1);
  if (!reader.Open()) {
    std::printf("FAIL: PmTilesReader::Open\n");
    return 1;
  }
  const orcmap::PmTilesHeader& header = reader.Header();
  std::printf("PMTiles v%u, z%u-z%u, %llu addressed tiles\n", header.version,
              header.min_zoom, header.max_zoom,
              static_cast<unsigned long long>(header.addressed_tiles_count));

  orcmap::MvtStreamScratch scratch;
  if (!orcmap::ReserveMvtStreamScratch(&scratch, 48 * 1024)) {
    std::printf("FAIL: stream scratch\n");
    return 1;
  }

  const orcmap::GeoBounds world{-180.0, -orcmap::kMercatorMaxLatDeg, 180.0,
                                orcmap::kMercatorMaxLatDeg};
  orcmap::LocationPickerLimits limits;
  limits.data_min_zoom = header.min_zoom;
  limits.data_max_zoom = header.max_zoom;
  const orcmap::LatLon pick{44.0462, -123.0220};  // Springfield, Oregon

  bool ok = true;
  const int sizes[][2] = {{1280, 720}, {930, 720}, {480, 320}};
  for (const auto& size : sizes) {
    orcmap::Viewport viewport;
    viewport.width_px = size[0];
    viewport.height_px = size[1];
    orcmap::LocationPicker picker;
    if (!picker.Begin(viewport, world, limits)) {
      std::printf("%dx%d: FAIL LocationPicker::Begin\n", size[0], size[1]);
      ok = false;
      continue;
    }
    std::printf("%dx%d: picker zoom z%u-z%u, opens at z%u\n", size[0],
                size[1], picker.MinZoom(), picker.MaxZoom(), picker.Zoom());
    const std::string prefix =
        ppm_dir.empty() ? std::string()
                        : ppm_dir + "/world-" + std::to_string(size[0]) + "x" +
                              std::to_string(size[1]);
    ok &= CheckFrame(reader, picker.View(), &scratch,
                     prefix.empty() ? prefix : prefix + "-open.ppm");

    double sx = 0.0, sy = 0.0;
    ok &= orcmap::ProjectLatLon(picker.View(), pick, &sx, &sy) &&
          picker.RecenterAtScreen(sx, sy);
    while (picker.CanZoomIn()) picker.ZoomIn();
    const orcmap::LatLon selected = picker.Selection();
    std::printf("  picked %.4f,%.4f at z%u\n", selected.lat_deg,
                selected.lon_deg, picker.Zoom());
    ok &= CheckFrame(reader, picker.View(), &scratch,
                     prefix.empty() ? prefix : prefix + "-picked.ppm");

    ok &= picker.DragByGesture(size[0] / 2.0, 0.0);
    ok &= CheckFrame(reader, picker.View(), &scratch, std::string());
  }

  std::printf("RESULT: %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
