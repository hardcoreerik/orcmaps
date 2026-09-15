#include "file_byte_source.hpp"
#include "framebuffer_target.hpp"
#include "orcmap/compression.hpp"
#include "orcmap/experimental/mvt_classify.hpp"
#include "orcmap/feature.hpp"
#include "orcmap/mvt.hpp"
#include "orcmap/mvt_translate.hpp"
#include "orcmap/pmtiles.hpp"
#include "orcmap/renderer.hpp"
#include "orcmap/style.hpp"
#include "orcmap/viewport.hpp"

#include <fstream>
#include <iterator>
#include <string>
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
}
