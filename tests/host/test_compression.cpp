#include "file_byte_source.hpp"
#include "framebuffer_target.hpp"
#include "orcmap/compression.hpp"
#include "orcmap/experimental/mvt_classify.hpp"
#include "orcmap/feature.hpp"
#include "orcmap/mvt.hpp"
#include "orcmap/mvt_translate.hpp"
#include "orcmap/mvt_stream.hpp"
#include "orcmap/pmtiles.hpp"
#include "orcmap/renderer.hpp"
#include "orcmap/style.hpp"
#include "orcmap/viewport.hpp"

#include <fstream>
#include <iterator>
#include <string>
#include <algorithm>
#include <vector>

#include "test_util.hpp"

namespace {

std::vector<uint8_t> ReadFile(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),
                               std::istreambuf_iterator<char>());
}

std::string Sibling(const std::string& path, const char* name) {
  const auto pos = path.find_last_of("/\\");
  if (pos == std::string::npos) return name;
  return path.substr(0, pos + 1) + name;
}

void TestNoneRoundTrip() {
  const uint8_t data[] = {1, 2, 3, 4};
  std::vector<uint8_t> out;
  ORCMAP_EXPECT_TRUE(orcmap::DecompressPayload(orcmap::Compression::kNone, data,
                                               sizeof(data), 16, &out));
  ORCMAP_EXPECT_EQ(out.size(), sizeof(data));
  ORCMAP_EXPECT_TRUE(out[0] == 1 && out[3] == 4);
}

void TestNoneEmpty() {
  std::vector<uint8_t> out{1, 2};
  ORCMAP_EXPECT_TRUE(orcmap::DecompressPayload(orcmap::Compression::kNone, nullptr,
                                               0, 8, &out));
  ORCMAP_EXPECT_EQ(out.size(), static_cast<size_t>(0));
}

void TestNoneBudgetTooSmall() {
  const uint8_t data[] = {1, 2, 3, 4};
  std::vector<uint8_t> out{9};
  ORCMAP_EXPECT_TRUE(!orcmap::DecompressPayload(orcmap::Compression::kNone, data,
                                                sizeof(data), 3, &out));
  ORCMAP_EXPECT_EQ(out.size(), static_cast<size_t>(0));
}

void TestValidGzip(const std::string& gz_path, const std::string& raw_path) {
  const auto gz = ReadFile(gz_path);
  const auto raw = ReadFile(raw_path);
  ORCMAP_EXPECT_TRUE(!gz.empty() && !raw.empty());
  std::vector<uint8_t> out;
  ORCMAP_EXPECT_TRUE(orcmap::DecompressPayload(orcmap::Compression::kGzip,
                                               gz.data(), gz.size(), 4096, &out));
  ORCMAP_EXPECT_EQ(out.size(), raw.size());
  ORCMAP_EXPECT_TRUE(out == raw);
}

void TestTruncatedGzip(const std::string& gz_path) {
  auto gz = ReadFile(gz_path);
  ORCMAP_EXPECT_TRUE(gz.size() > 10);
  gz.resize(gz.size() / 2);
  std::vector<uint8_t> out{1};
  ORCMAP_EXPECT_TRUE(!orcmap::DecompressPayload(orcmap::Compression::kGzip,
                                                gz.data(), gz.size(), 4096, &out));
  ORCMAP_EXPECT_EQ(out.size(), static_cast<size_t>(0));
}

void TestInvalidGzipHeader() {
  const uint8_t junk[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                          0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
                          0x10, 0x11};
  std::vector<uint8_t> out{1};
  ORCMAP_EXPECT_TRUE(!orcmap::DecompressPayload(orcmap::Compression::kGzip, junk,
                                                sizeof(junk), 4096, &out));
  ORCMAP_EXPECT_EQ(out.size(), static_cast<size_t>(0));
}

void TestGzipBudgetTooSmall(const std::string& gz_path) {
  const auto gz = ReadFile(gz_path);
  std::vector<uint8_t> out{1};
  ORCMAP_EXPECT_TRUE(!orcmap::DecompressPayload(orcmap::Compression::kGzip,
                                                gz.data(), gz.size(), 8, &out));
  ORCMAP_EXPECT_EQ(out.size(), static_cast<size_t>(0));
}

void TestUnsupportedBrotliZstd() {
  const uint8_t data[] = {1, 2, 3};
  std::vector<uint8_t> out{1};
  ORCMAP_EXPECT_TRUE(!orcmap::DecompressPayload(orcmap::Compression::kBrotli, data,
                                                sizeof(data), 64, &out));
  ORCMAP_EXPECT_EQ(out.size(), static_cast<size_t>(0));
  out = {1};
  ORCMAP_EXPECT_TRUE(!orcmap::DecompressPayload(orcmap::Compression::kZstd, data,
                                                sizeof(data), 64, &out));
  ORCMAP_EXPECT_EQ(out.size(), static_cast<size_t>(0));
  out = {1};
  ORCMAP_EXPECT_TRUE(!orcmap::DecompressPayload(orcmap::Compression::kUnknown, data,
                                                sizeof(data), 64, &out));
  ORCMAP_EXPECT_EQ(out.size(), static_cast<size_t>(0));
}

void TestEmptyGzipInput() {
  std::vector<uint8_t> out{1};
  ORCMAP_EXPECT_TRUE(!orcmap::DecompressPayload(orcmap::Compression::kGzip, nullptr,
                                                0, 64, &out));
  ORCMAP_EXPECT_EQ(out.size(), static_cast<size_t>(0));
}

void TestCorruptGzipPayload(const std::string& gz_path) {
  auto gz = ReadFile(gz_path);
  ORCMAP_EXPECT_TRUE(gz.size() > 20);
  gz[15] ^= 0xFF;
  std::vector<uint8_t> out{1};
  ORCMAP_EXPECT_TRUE(!orcmap::DecompressPayload(orcmap::Compression::kGzip,
                                                gz.data(), gz.size(), 4096, &out));
  ORCMAP_EXPECT_EQ(out.size(), static_cast<size_t>(0));
}

// STREAMING inflate must be byte-identical to fetch-then-inflate. This is
// the gate on the change that let a no-PSRAM board render mid-zoom tiles: it
// halves the large-allocation requirement, and it must not alter one byte of
// output while doing so.
// The forward-only inflating stream must reproduce the inflated bytes
// exactly, in any read pattern, while never holding the whole payload.
void TestInflatingStreamMatchesBufferedInflate(const std::string& gzip_pmtiles) {
  orcmap::host::FileByteSource source(gzip_pmtiles);
  ORCMAP_EXPECT_TRUE(source.Valid());
  orcmap::PmTilesReader reader(&source);
  ORCMAP_EXPECT_TRUE(reader.Open());

  std::vector<uint8_t> want;
  ORCMAP_EXPECT_TRUE(reader.GetTileInflated(0, 0, 0, 1u << 20, &want));
  ORCMAP_EXPECT_TRUE(!want.empty());

  uint64_t offset = 0;
  uint32_t length = 0;
  ORCMAP_EXPECT_TRUE(reader.LocateTileForTest(0, 0, 0, &offset, &length));

  const auto pull = [](void* ctx, uint64_t off, uint8_t* dst,
                       size_t len) -> size_t {
    return static_cast<orcmap::ByteSource*>(ctx)->Read(off, dst, len);
  };

  // Awkward read sizes on purpose: 1, then 7, then 13, ... so reads cross
  // the window's refill boundary at unaligned offsets.
  orcmap::InflatingByteStream stream;
  ORCMAP_EXPECT_TRUE(stream.Begin(reader.Header().tile_compression, pull,
                                  &source, offset, length));
  std::vector<uint8_t> got;
  const size_t sizes[] = {1, 7, 13, 64, 3, 255, 2};
  size_t si = 0;
  while (got.size() < want.size()) {
    size_t n = sizes[si++ % 7];
    if (n > want.size() - got.size()) n = want.size() - got.size();
    std::vector<uint8_t> buf(n);
    ORCMAP_EXPECT_TRUE(stream.Read(buf.data(), n));
    got.insert(got.end(), buf.begin(), buf.end());
  }
  ORCMAP_EXPECT_TRUE(got == want);
  ORCMAP_EXPECT_TRUE(stream.consumed() == want.size());
  ORCMAP_EXPECT_TRUE(stream.AtEnd());
  // Reading past the end fails rather than returning garbage.
  uint8_t past = 0;
  ORCMAP_EXPECT_TRUE(!stream.Read(&past, 1));

  // Skip must land in exactly the same place as reading.
  orcmap::InflatingByteStream skipper;
  ORCMAP_EXPECT_TRUE(skipper.Begin(reader.Header().tile_compression, pull,
                                   &source, offset, length));
  const uint64_t jump = want.size() / 2;
  ORCMAP_EXPECT_TRUE(skipper.Skip(jump));
  std::vector<uint8_t> tail(want.size() - jump);
  ORCMAP_EXPECT_TRUE(skipper.Read(tail.data(), tail.size()));
  ORCMAP_EXPECT_TRUE(std::equal(tail.begin(), tail.end(), want.begin() + jump));

  // Re-Begin restarts from the top, which is what a two-pass parser needs.
  ORCMAP_EXPECT_TRUE(skipper.Begin(reader.Header().tile_compression, pull,
                                   &source, offset, length));
  std::vector<uint8_t> again(want.size());
  ORCMAP_EXPECT_TRUE(skipper.Read(again.data(), again.size()));
  ORCMAP_EXPECT_TRUE(again == want);
}

// END-TO-END STREAMING PARSE must emit exactly what the buffered decoder
// produces. This is the gate on the change that removes the whole-tile
// buffer: if the two ever diverged, a board with PSRAM and a board without
// would draw different maps from the same pack.
struct ParseCapture {
  std::vector<orcmap::MvtFeature> features;
  std::vector<std::string> layers;
  std::vector<uint32_t> extents;
};

bool CaptureParsed(const orcmap::MvtLayer& layer,
                   const orcmap::MvtFeature& feature, void* ctx) {
  ParseCapture& cap = *static_cast<ParseCapture*>(ctx);
  cap.layers.push_back(layer.name);
  cap.extents.push_back(layer.extent);
  cap.features.push_back(feature);
  return true;
}

void TestStreamedParseMatchesBufferedDecode(const std::string& gzip_pmtiles) {
  orcmap::host::FileByteSource source(gzip_pmtiles);
  ORCMAP_EXPECT_TRUE(source.Valid());
  orcmap::PmTilesReader reader(&source);
  ORCMAP_EXPECT_TRUE(reader.Open());

  // Reference: inflate the whole tile, then decode it.
  std::vector<uint8_t> raw;
  ORCMAP_EXPECT_TRUE(reader.GetTileInflated(0, 0, 0, 1u << 20, &raw));
  orcmap::MvtTile buffered;
  ORCMAP_EXPECT_TRUE(
      orcmap::DecodeMvtTile(raw.data(), raw.size(), &buffered));

  uint64_t offset = 0;
  uint32_t length = 0;
  ORCMAP_EXPECT_TRUE(reader.LocateTileForTest(0, 0, 0, &offset, &length));
  const auto pull = [](void* ctx, uint64_t off, uint8_t* dst,
                       size_t len) -> size_t {
    return static_cast<orcmap::ByteSource*>(ctx)->Read(off, dst, len);
  };

  ParseCapture cap;
  orcmap::MvtDecodeOptions options;
  options.feature_sink = &CaptureParsed;
  options.feature_sink_ctx = &cap;
  orcmap::MvtStreamScratch scratch;
  ORCMAP_EXPECT_TRUE(orcmap::StreamMvtTile(reader.Header().tile_compression,
                                           pull, &source, offset, length,
                                           options, &scratch));

  size_t expected = 0;
  for (const orcmap::MvtLayer& layer : buffered.layers) {
    expected += layer.features.size();
  }
  ORCMAP_EXPECT_TRUE(expected > 0);
  ORCMAP_EXPECT_EQ(static_cast<int>(cap.features.size()),
                   static_cast<int>(expected));

  size_t i = 0;
  for (const orcmap::MvtLayer& layer : buffered.layers) {
    for (const orcmap::MvtFeature& want : layer.features) {
      const orcmap::MvtFeature& got = cap.features[i];
      ORCMAP_EXPECT_EQ(cap.layers[i], layer.name);
      ORCMAP_EXPECT_TRUE(cap.extents[i] == layer.extent);
      ORCMAP_EXPECT_TRUE(got.id == want.id);
      ORCMAP_EXPECT_TRUE(got.geom_type == want.geom_type);
      ORCMAP_EXPECT_EQ(static_cast<int>(got.geometry.size()),
                       static_cast<int>(want.geometry.size()));
      for (size_t r = 0; r < want.geometry.size(); ++r) {
        ORCMAP_EXPECT_EQ(static_cast<int>(got.geometry[r].size()),
                         static_cast<int>(want.geometry[r].size()));
        for (size_t k = 0; k < want.geometry[r].size(); ++k) {
          ORCMAP_EXPECT_TRUE(got.geometry[r][k].x == want.geometry[r][k].x);
          ORCMAP_EXPECT_TRUE(got.geometry[r][k].y == want.geometry[r][k].y);
        }
      }
      // Attributes prove the two-pass table resolution worked: these indices
      // are only resolvable because pass 1 collected keys/values that the
      // wire format places AFTER the features referencing them.
      ORCMAP_EXPECT_TRUE(got.attribute_keys == want.attribute_keys);
      ORCMAP_EXPECT_EQ(static_cast<int>(got.attribute_values.size()),
                       static_cast<int>(want.attribute_values.size()));
      for (size_t v = 0; v < want.attribute_values.size(); ++v) {
        ORCMAP_EXPECT_TRUE(got.attribute_values[v] == want.attribute_values[v]);
      }
      ++i;
    }
  }

  // The scratch is reusable: a second parse must give the same answer.
  ParseCapture again;
  options.feature_sink_ctx = &again;
  ORCMAP_EXPECT_TRUE(orcmap::StreamMvtTile(reader.Header().tile_compression,
                                           pull, &source, offset, length,
                                           options, &scratch));
  ORCMAP_EXPECT_EQ(static_cast<int>(again.features.size()),
                   static_cast<int>(expected));

  // A refusing sink aborts rather than truncating silently.
  struct Refuse {
    static bool Sink(const orcmap::MvtLayer&, const orcmap::MvtFeature&,
                     void* ctx) {
      ++*static_cast<int*>(ctx);
      return false;
    }
  };
  int calls = 0;
  orcmap::MvtDecodeOptions refusing;
  refusing.feature_sink = &Refuse::Sink;
  refusing.feature_sink_ctx = &calls;
  ORCMAP_EXPECT_TRUE(!orcmap::StreamMvtTile(reader.Header().tile_compression,
                                            pull, &source, offset, length,
                                            refusing, &scratch));
  ORCMAP_EXPECT_EQ(calls, 1);

  // A sink is mandatory -- this parser has no accumulating mode.
  orcmap::MvtDecodeOptions no_sink;
  ORCMAP_EXPECT_TRUE(!orcmap::StreamMvtTile(reader.Header().tile_compression,
                                            pull, &source, offset, length,
                                            no_sink, &scratch));
}

void TestStreamingInflateMatchesTwoStep(const std::string& gzip_pmtiles) {
  orcmap::host::FileByteSource source(gzip_pmtiles);
  ORCMAP_EXPECT_TRUE(source.Valid());
  orcmap::PmTilesReader reader(&source);
  ORCMAP_EXPECT_TRUE(reader.Open());

  // Reference: the old two-step path.
  std::vector<uint8_t> stored;
  ORCMAP_EXPECT_TRUE(reader.GetTile(0, 0, 0, &stored));
  std::vector<uint8_t> two_step;
  ORCMAP_EXPECT_TRUE(orcmap::DecompressPayload(
      reader.Header().tile_compression, stored.data(), stored.size(),
      1u << 20, &two_step));
  ORCMAP_EXPECT_TRUE(!two_step.empty());

  // Streaming, into a fresh buffer.
  std::vector<uint8_t> streamed;
  ORCMAP_EXPECT_TRUE(
      reader.GetTileInflated(0, 0, 0, 1u << 20, &streamed));
  ORCMAP_EXPECT_TRUE(streamed == two_step);

  // Streaming into a REUSED buffer that already holds larger content must
  // still produce exactly the tile, not a mixture -- this is how the
  // embedded path uses it.
  std::vector<uint8_t> reused(4096, 0xAB);
  reused.reserve(1u << 16);
  const size_t capacity_before = reused.capacity();
  ORCMAP_EXPECT_TRUE(reader.GetTileInflated(0, 0, 0, 1u << 20, &reused));
  ORCMAP_EXPECT_TRUE(reused == two_step);
  // Reuse must not have thrown the reservation away.
  ORCMAP_EXPECT_TRUE(reused.capacity() >= capacity_before);

  // A budget below the real inflated size is refused, not truncated.
  std::vector<uint8_t> too_small;
  ORCMAP_EXPECT_TRUE(
      !reader.GetTileInflated(0, 0, 0, two_step.size() - 1, &too_small));
  ORCMAP_EXPECT_TRUE(too_small.empty());

  // An absent tile is reported as absent by both paths, consistently.
  ORCMAP_EXPECT_TRUE(!reader.TileExists(14, 1, 1));
  std::vector<uint8_t> absent;
  ORCMAP_EXPECT_TRUE(!reader.GetTileInflated(14, 1, 1, 1u << 20, &absent));
  ORCMAP_EXPECT_TRUE(reader.TileExists(0, 0, 0));
}

void TestGzipPmtilesToPixels(const std::string& gzip_pmtiles,
                             const std::string& raw_mvt) {
  orcmap::host::FileByteSource source(gzip_pmtiles);
  ORCMAP_EXPECT_TRUE(source.Valid());
  orcmap::PmTilesReader reader(&source);
  ORCMAP_EXPECT_TRUE(reader.Open());
  ORCMAP_EXPECT_TRUE(reader.Header().tile_compression ==
                     orcmap::Compression::kGzip);

  std::vector<uint8_t> stored;
  ORCMAP_EXPECT_TRUE(reader.GetTile(0, 0, 0, &stored));
  ORCMAP_EXPECT_TRUE(!stored.empty());

  std::vector<uint8_t> raw;
  ORCMAP_EXPECT_TRUE(orcmap::DecompressPayload(reader.Header().tile_compression,
                                               stored.data(), stored.size(),
                                               65536, &raw));
  const auto expected = ReadFile(raw_mvt);
  ORCMAP_EXPECT_TRUE(raw == expected);

  orcmap::MvtTile mvt;
  ORCMAP_EXPECT_TRUE(orcmap::DecodeMvtTile(raw.data(), raw.size(), &mvt));
  orcmap::FeatureTile features;
  ORCMAP_EXPECT_TRUE(orcmap::TranslateMvtToFeatureTile(mvt, &features));
  ORCMAP_EXPECT_TRUE(orcmap::experimental::AssignFeatureKinds(&features));

  orcmap::host::FramebufferTarget fb(32, 32);
  orcmap::Viewport vp;
  vp.center_lat_deg = 0;
  vp.center_lon_deg = 0;
  vp.zoom = 0;
  vp.width_px = 32;
  vp.height_px = 32;
  vp.tile_size_px = 32;
  ORCMAP_EXPECT_TRUE(
      orcmap::ClearMapBackground(vp, orcmap::styles::OrcSdrDark(), &fb));
  ORCMAP_EXPECT_TRUE(orcmap::RenderFeatureTile(
      features, orcmap::TileId{0, 0, 0}, vp, orcmap::styles::OrcSdrDark(), &fb));
  ORCMAP_EXPECT_TRUE(fb.At(0, 0) == orcmap::Color::Rgb(8, 10, 12));
}

}  // namespace

void RunCompressionTests(const std::string& pmtiles_path,
                         const std::string& mvt_path) {
  const std::string gz_path = Sibling(mvt_path, "tiny.mvt.gz");
  const std::string gzip_pmtiles = Sibling(pmtiles_path, "tiny-gzip.pmtiles");
  TestNoneRoundTrip();
  TestNoneEmpty();
  TestNoneBudgetTooSmall();
  TestValidGzip(gz_path, mvt_path);
  TestTruncatedGzip(gz_path);
  TestInvalidGzipHeader();
  TestGzipBudgetTooSmall(gz_path);
  TestUnsupportedBrotliZstd();
  TestEmptyGzipInput();
  TestCorruptGzipPayload(gz_path);
  TestGzipPmtilesToPixels(gzip_pmtiles, mvt_path);
  TestStreamingInflateMatchesTwoStep(gzip_pmtiles);
  TestInflatingStreamMatchesBufferedInflate(gzip_pmtiles);
  TestStreamedParseMatchesBufferedDecode(gzip_pmtiles);
}
