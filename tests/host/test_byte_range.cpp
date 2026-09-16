#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "orcmap/byte_range.hpp"
#include "orcmap/pmtiles.hpp"
#include "file_byte_source.hpp"
#include "test_util.hpp"

namespace {

using orcmap::ClampReadRange;

// The arithmetic every ByteSource adapter depends on. It matters most for a
// flash-backed source: an unclamped read there leaves the partition entirely
// rather than just returning junk from a file.
void TestReadsFullyInsideAreUnchanged() {
  size_t length = 0;
  ORCMAP_EXPECT_TRUE(ClampReadRange(0, 128, 4096, &length));
  ORCMAP_EXPECT_EQ(length, static_cast<size_t>(128));

  ORCMAP_EXPECT_TRUE(ClampReadRange(1000, 24, 4096, &length));
  ORCMAP_EXPECT_EQ(length, static_cast<size_t>(24));
}

void TestReadAtTheVeryStart() {
  size_t length = 0;
  ORCMAP_EXPECT_TRUE(ClampReadRange(0, 1, 4096, &length));
  ORCMAP_EXPECT_EQ(length, static_cast<size_t>(1));
  // A PMTiles header is 127 bytes and is read from offset 0, so this is the
  // first read any archive performs.
  ORCMAP_EXPECT_TRUE(ClampReadRange(0, 127, 4096, &length));
  ORCMAP_EXPECT_EQ(length, static_cast<size_t>(127));
}

void TestReadEndingExactlyAtTheEnd() {
  size_t length = 0;
  ORCMAP_EXPECT_TRUE(ClampReadRange(4096 - 16, 16, 4096, &length));
  ORCMAP_EXPECT_EQ(length, static_cast<size_t>(16));
  ORCMAP_EXPECT_TRUE(ClampReadRange(4095, 1, 4096, &length));
  ORCMAP_EXPECT_EQ(length, static_cast<size_t>(1));
}

void TestReadStraddlingTheEndIsShortened() {
  size_t length = 0;
  // 8 bytes available, 64 asked for.
  ORCMAP_EXPECT_TRUE(ClampReadRange(4096 - 8, 64, 4096, &length));
  ORCMAP_EXPECT_EQ(length, static_cast<size_t>(8));

  // The realistic case: a 6 MiB partition holding a 4.61 MiB archive, read
  // near the end of the partition.
  const uint64_t partition = 6u * 1024u * 1024u;
  ORCMAP_EXPECT_TRUE(ClampReadRange(partition - 100, 4096, partition, &length));
  ORCMAP_EXPECT_EQ(length, static_cast<size_t>(100));
}

void TestReadAtOrPastTheEndReadsNothing() {
  size_t length = 0;
  ORCMAP_EXPECT_TRUE(!ClampReadRange(4096, 1, 4096, &length));
  ORCMAP_EXPECT_EQ(length, static_cast<size_t>(0));
  ORCMAP_EXPECT_TRUE(!ClampReadRange(4097, 16, 4096, &length));
  ORCMAP_EXPECT_EQ(length, static_cast<size_t>(0));
  ORCMAP_EXPECT_TRUE(!ClampReadRange(1u << 30, 16, 4096, &length));
  ORCMAP_EXPECT_EQ(length, static_cast<size_t>(0));
}

void TestZeroLengthAndZeroSize() {
  size_t length = 0;
  ORCMAP_EXPECT_TRUE(!ClampReadRange(0, 0, 4096, &length));
  ORCMAP_EXPECT_EQ(length, static_cast<size_t>(0));
  // An absent partition reports size 0; every read must decline.
  ORCMAP_EXPECT_TRUE(!ClampReadRange(0, 16, 0, &length));
  ORCMAP_EXPECT_EQ(length, static_cast<size_t>(0));
}

void TestHostileOffsetCannotWrapAround() {
  // offset + length would overflow to a small number if the sum were ever
  // formed, making an out-of-range read look valid.
  size_t length = 0;
  const uint64_t near_top = std::numeric_limits<uint64_t>::max() - 4;
  ORCMAP_EXPECT_TRUE(!ClampReadRange(near_top, 64, 4096, &length));
  ORCMAP_EXPECT_EQ(length, static_cast<size_t>(0));
  ORCMAP_EXPECT_TRUE(
      !ClampReadRange(std::numeric_limits<uint64_t>::max(), 1, 4096, &length));
  ORCMAP_EXPECT_EQ(length, static_cast<size_t>(0));
}

void TestNullOutputIsRejected() {
  ORCMAP_EXPECT_TRUE(!ClampReadRange(0, 16, 4096, nullptr));
}

// A ByteSource whose backing store is larger than the archive written into
// it, which is exactly what a 6 MiB flash partition holding a 4.61 MiB pack
// looks like. Reads are clamped with the same helper the real partition
// adapter uses.
//
// This is not a duplicate of the existing PMTiles tests: those read an
// archive whose source size equals the archive size. What is checked here is
// that a source reporting MORE bytes than the archive still opens, because
// that is the one structural difference an embedded pack introduces.
class OversizedByteSource : public orcmap::ByteSource {
 public:
  OversizedByteSource(std::vector<uint8_t> archive, uint64_t reported_size)
      : archive_(std::move(archive)), reported_size_(reported_size) {}

  size_t Read(uint64_t offset, void* destination, size_t length) override {
    size_t readable = 0;
    if (!ClampReadRange(offset, length, reported_size_, &readable)) return 0;
    uint8_t* out = static_cast<uint8_t*>(destination);
    for (size_t i = 0; i < readable; ++i) {
      const uint64_t at = offset + i;
      // Past the real archive the partition holds erased flash.
      out[i] = at < archive_.size() ? archive_[static_cast<size_t>(at)] : 0xFF;
    }
    return readable;
  }
  uint64_t Size() const override { return reported_size_; }
  bool Valid() const override { return true; }

 private:
  std::vector<uint8_t> archive_;
  uint64_t reported_size_ = 0;
};

void TestArchiveOpensFromAnOversizedSource(const std::string& fixture_path) {
  std::vector<uint8_t> archive;
  {
    orcmap::host::FileByteSource file(fixture_path.c_str());
    ORCMAP_EXPECT_TRUE(file.Valid());
    archive.resize(static_cast<size_t>(file.Size()));
    if (!archive.empty()) {
      ORCMAP_EXPECT_EQ(file.Read(0, archive.data(), archive.size()),
                       archive.size());
    }
  }
  ORCMAP_EXPECT_TRUE(!archive.empty());

  // Report a partition a good deal larger than the archive, padded with
  // erased flash, and confirm the header still parses and tiles still read.
  const uint64_t padded = static_cast<uint64_t>(archive.size()) + 1536u * 1024u;
  OversizedByteSource source(archive, padded);
  orcmap::PmTilesReader reader(&source);
  ORCMAP_EXPECT_TRUE(reader.Open());
  ORCMAP_EXPECT_TRUE(source.Size() > static_cast<uint64_t>(archive.size()));
  ORCMAP_EXPECT_TRUE(reader.Header().max_zoom >= reader.Header().min_zoom);
}

}  // namespace

void RunByteRangeTests(const std::string& fixture_path) {
  TestReadsFullyInsideAreUnchanged();
  TestReadAtTheVeryStart();
  TestReadEndingExactlyAtTheEnd();
  TestReadStraddlingTheEndIsShortened();
  TestReadAtOrPastTheEndReadsNothing();
  TestZeroLengthAndZeroSize();
  TestHostileOffsetCannotWrapAround();
  TestNullOutputIsRejected();
  TestArchiveOpensFromAnOversizedSource(fixture_path);
}
