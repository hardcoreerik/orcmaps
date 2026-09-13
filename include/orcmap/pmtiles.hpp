#pragma once

#include <cstdint>
#include <vector>

#include "orcmap/byte_source.hpp"

namespace orcmap {

// PMTiles v3 container reader. Spec: github.com/protomaps/PMTiles
// (spec/v3/spec.md). See docs/FORMAT_DECISION.md for why this container
// was chosen.
//
// Bounded-memory by construction: Open() holds only the 127-byte header
// and the decompressed root directory (<=16,257 bytes compressed per
// spec) in memory. Leaf directories and tile bytes are fetched on demand
// via the ByteSource and never accumulate -- this class never loads a
// whole archive into RAM regardless of archive size.

enum class Compression : uint8_t {
  kUnknown = 0,
  kNone = 1,
  kGzip = 2,
  kBrotli = 3,
  kZstd = 4,
};

enum class TileType : uint8_t {
  kUnknown = 0,
  kMvt = 1,
  kPng = 2,
  kJpeg = 3,
  kWebp = 4,
  kAvif = 5,
};

struct PmTilesHeader {
  uint8_t version = 0;

  uint64_t root_dir_offset = 0;
  uint64_t root_dir_length = 0;
  uint64_t metadata_offset = 0;
  uint64_t metadata_length = 0;
  uint64_t leaf_dirs_offset = 0;
  uint64_t leaf_dirs_length = 0;
  uint64_t tile_data_offset = 0;
  uint64_t tile_data_length = 0;

  uint64_t addressed_tiles_count = 0;
  uint64_t tile_entries_count = 0;
  uint64_t tile_contents_count = 0;

  bool clustered = false;
  Compression internal_compression = Compression::kUnknown;
  Compression tile_compression = Compression::kUnknown;
  TileType tile_type = TileType::kUnknown;

  uint8_t min_zoom = 0;
  uint8_t max_zoom = 0;

  int32_t min_lon_e7 = 0;
  int32_t min_lat_e7 = 0;
  int32_t max_lon_e7 = 0;
  int32_t max_lat_e7 = 0;

  uint8_t center_zoom = 0;
  int32_t center_lon_e7 = 0;
  int32_t center_lat_e7 = 0;
};

// Maps z/x/y to its PMTiles Hilbert-curve tile ID (a single uint64 ordering
// tiles by spatial locality across a stacked pyramid of zoom levels: z0 is
// ID 0, z1 occupies IDs 1-4, etc.). Exposed standalone because the pack
// builder needs the same function to write a directory the reader can
// binary-search.
uint64_t ZxyToTileId(uint8_t z, uint32_t x, uint32_t y);

class PmTilesReader {
 public:
  // `source` must outlive this reader and any data returned by it (this
  // reader does not copy source's lifetime, only reads through it).
  explicit PmTilesReader(ByteSource* source);

  // Parses the header and loads+decompresses the root directory. Returns
  // false (and leaves IsOpen() false) on any structural problem: bad magic,
  // unsupported version, truncated file, oversized/corrupt root directory,
  // or decompression failure. Never throws.
  bool Open();

  bool IsOpen() const { return open_; }
  const PmTilesHeader& Header() const { return header_; }

  // Reads the archive's metadata JSON bytes (small -- read whole, unlike
  // tile data). Returns false if not open or on I/O error.
  bool ReadMetadata(std::vector<uint8_t>* out) const;

  // Looks up one tile by z/x/y. Returns false if the tile is absent from
  // this archive (sparse coverage -- not an error) or on I/O/format error;
  // callers distinguish the two via IsOpen() staying true either way. On
  // success `out` holds the tile bytes exactly as stored -- still
  // compressed per Header().tile_compression, since decompression belongs
  // to the render layer, which owns its own reusable scratch buffer.
  bool GetTile(uint8_t z, uint32_t x, uint32_t y,
               std::vector<uint8_t>* out) const;

 private:
  struct DirEntry {
    uint64_t tile_id = 0;
    uint32_t run_length = 0;
    uint32_t length = 0;
    uint64_t offset = 0;  // Absolute offset within tile-data/leaf-dir section.
  };

  // Reads `length` bytes at `offset`, decompresses per
  // header_.internal_compression if needed, and parses the PMTiles
  // directory entry format into `out`. Used for both the root directory
  // and any leaf directory.
  bool ReadDirectory(uint64_t offset, uint64_t length,
                      std::vector<DirEntry>* out) const;

  // Binary-searches a parsed, tile_id-sorted directory for `tile_id`.
  // Returns false if no entry could contain this tile_id at all. On true,
  // `is_leaf` tells the caller whether `found` is a tile entry
  // (run_length > 0) or a leaf-directory pointer (run_length == 0).
  static bool FindEntry(const std::vector<DirEntry>& dir, uint64_t tile_id,
                        DirEntry* found, bool* is_leaf);

  ByteSource* source_;
  bool open_ = false;
  PmTilesHeader header_;
  std::vector<DirEntry> root_dir_;
};

}  // namespace orcmap
