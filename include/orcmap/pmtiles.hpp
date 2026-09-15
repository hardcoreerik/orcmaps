#pragma once

#include <cstdint>
#include <vector>

#include "orcmap/byte_source.hpp"
#include "orcmap/compression.hpp"
#include "orcmap/mvt_stream.hpp"

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
  // compressed per Header().tile_compression. Call DecompressPayload()
  // (include/orcmap/compression.hpp) before DecodeMvtTile().
  bool GetTile(uint8_t z, uint32_t x, uint32_t y,
               std::vector<uint8_t>* out) const;

  // Looks up a tile AND inflates it, streaming the compressed bytes out of
  // the ByteSource so the compressed payload is never resident alongside the
  // inflated payload. Use this instead of GetTile + DecompressPayload on
  // memory-constrained targets: it halves the large-allocation requirement,
  // which is what makes mid-zoom tiles possible without PSRAM.
  //
  // `out` is resized to the exact inflated size; reserve its capacity once
  // and reuse it across tiles. Returns false if the tile is absent (not an
  // error -- sparse coverage), on I/O or format error, or if the inflated
  // size would exceed `max_output_size`. Distinguish absence from failure
  // with TileExists().
  bool GetTileInflated(uint8_t z, uint32_t x, uint32_t y,
                       size_t max_output_size,
                       std::vector<uint8_t>* out) const;

  // True if this archive stores the tile, without reading its bytes.
  bool TileExists(uint8_t z, uint32_t x, uint32_t y) const;

  // Parses a tile straight from the archive, emitting features to
  // options.feature_sink, WITHOUT ever holding the inflated tile. This is
  // the path a board with no PSRAM needs: see include/orcmap/mvt_stream.hpp
  // for the measured reason. Returns false if the tile is absent or the
  // payload is malformed; `scratch` should be owned and reused by the
  // caller.
  bool StreamTile(uint8_t z, uint32_t x, uint32_t y,
                  const MvtDecodeOptions& options,
                  MvtStreamScratch* scratch) const;

  // Resolves a tile to its absolute archive offset and stored length, for a
  // caller that wants to stream the compressed bytes itself. Exposed for the
  // streaming parser and its tests.
  bool LocateTileForTest(uint8_t z, uint32_t x, uint32_t y, uint64_t* offset,
                         uint32_t* length) const {
    return LocateTile(z, x, y, offset, length);
  }

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

  // Directory walk shared by GetTile and GetTileInflated: resolves z/x/y to
  // an absolute archive offset and stored length. Null outputs make it an
  // existence check.
  bool LocateTile(uint8_t z, uint32_t x, uint32_t y, uint64_t* offset,
                  uint32_t* length) const;

  // Binary-searches a parsed, tile_id-sorted directory for `tile_id`.
  // Returns false if no entry could contain this tile_id at all. On true,
  // `is_leaf` tells the caller whether `found` is a tile entry
  // (run_length > 0) or a leaf-directory pointer (run_length == 0).
  static bool FindEntry(const std::vector<DirEntry>& dir, uint64_t tile_id,
                        DirEntry* found, bool* is_leaf);

  // Returns a parsed leaf directory, reading and decompressing it only if it
  // is not already cached. Tiles in one frame are spatially adjacent and
  // PMTiles orders entries by Hilbert tile id, so a whole frame usually hits
  // the same one or two leaf directories; without this, every tile lookup
  // re-read AND re-inflated the same directory from storage.
  //
  // Returns nullptr on I/O or format error. The pointer is valid until the
  // next call that misses the cache.
  const std::vector<DirEntry>* LeafDirectory(uint64_t offset,
                                             uint64_t length) const;

  // Small round-robin cache. Four slots covers a frame that straddles a
  // directory boundary while keeping worst-case memory bounded and
  // predictable, which matters more than hit rate on an embedded target.
  static constexpr int kLeafCacheSlots = 4;
  struct LeafCacheSlot {
    bool valid = false;
    uint64_t offset = 0;
    uint64_t length = 0;
    std::vector<DirEntry> entries;
  };

  ByteSource* source_;
  bool open_ = false;
  PmTilesHeader header_;
  std::vector<DirEntry> root_dir_;

  // Mutable because GetTile() is logically const: caching changes speed,
  // not observable results.
  mutable LeafCacheSlot leaf_cache_[kLeafCacheSlots];
  mutable int leaf_cache_next_ = 0;
};

}  // namespace orcmap
