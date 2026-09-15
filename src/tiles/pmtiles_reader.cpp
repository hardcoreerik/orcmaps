#include "orcmap/pmtiles.hpp"

#include <algorithm>
#include <cstring>

namespace orcmap {

namespace {

constexpr size_t kHeaderSize = 127;
constexpr char kMagic[7] = {'P', 'M', 'T', 'i', 'l', 'e', 's'};
// Spec: the root directory (compressed) must fit in this many bytes.
constexpr uint64_t kMaxRootDirBytes = 16384;
// Guard against a corrupt/hostile length field forcing a huge allocation
// (leaf directories are not spec-capped the way the root is).
constexpr uint64_t kMaxDirectoryReadBytes = 64u * 1024 * 1024;

uint64_t ReadU64LE(const uint8_t* p) {
  uint64_t v = 0;
  for (int i = 7; i >= 0; --i) v = (v << 8) | p[i];
  return v;
}

int32_t ReadI32LE(const uint8_t* p) {
  uint32_t v = static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
               (static_cast<uint32_t>(p[2]) << 16) |
               (static_cast<uint32_t>(p[3]) << 24);
  return static_cast<int32_t>(v);
}

// Unsigned LEB128, as used throughout the PMTiles directory format.
uint64_t ReadVarint(const uint8_t* data, size_t len, size_t* pos, bool* ok) {
  uint64_t result = 0;
  int shift = 0;
  while (*pos < len) {
    uint8_t byte = data[(*pos)++];
    result |= static_cast<uint64_t>(byte & 0x7f) << shift;
    if ((byte & 0x80) == 0) {
      *ok = true;
      return result;
    }
    shift += 7;
    if (shift > 63) break;
  }
  *ok = false;
  return 0;
}

}  // namespace

// --- Hilbert curve tile addressing ------------------------------------
//
// PMTiles orders tiles by a Hilbert curve within each zoom level, then
// stacks zoom levels so tile IDs increase monotonically with zoom and
// preserve spatial locality within a level (nearby tiles get nearby IDs,
// letting sparse/uniform regions collapse into long directory run-lengths
// -- see docs/FORMAT_DECISION.md). This is the standard integer Hilbert
// d2xy/xy2d algorithm (public, e.g. Wikipedia "Hilbert curve"), applied
// per the PMTiles spec's level-stacking rule, not code copied from any
// PMTiles implementation.

namespace {

uint64_t HilbertXyToIndex(uint8_t level, uint32_t x, uint32_t y) {
  uint64_t d = 0;
  for (int32_t s = (1 << (level - 1)); s > 0; s >>= 1) {
    const uint32_t rx = (x & static_cast<uint32_t>(s)) > 0 ? 1u : 0u;
    const uint32_t ry = (y & static_cast<uint32_t>(s)) > 0 ? 1u : 0u;
    d += static_cast<uint64_t>(s) * static_cast<uint64_t>(s) *
         ((3u * rx) ^ ry);
    // Rotate the quadrant.
    if (ry == 0) {
      if (rx == 1) {
        x = static_cast<uint32_t>(s - 1 - static_cast<int32_t>(x));
        y = static_cast<uint32_t>(s - 1 - static_cast<int32_t>(y));
      }
      std::swap(x, y);
    }
  }
  return d;
}

// Sum of tile counts for all zoom levels below `z`: (4^z - 1) / 3.
uint64_t TilesBeforeLevel(uint8_t z) {
  uint64_t sum = 0;
  uint64_t level_size = 1;
  for (uint8_t i = 0; i < z; ++i) {
    sum += level_size;
    level_size *= 4;
  }
  return sum;
}

}  // namespace

uint64_t ZxyToTileId(uint8_t z, uint32_t x, uint32_t y) {
  if (z == 0) return 0;
  return TilesBeforeLevel(z) + HilbertXyToIndex(z, x, y);
}

// --- PmTilesReader -------------------------------------------------------

PmTilesReader::PmTilesReader(ByteSource* source) : source_(source) {}

bool PmTilesReader::Open() {
  open_ = false;
  if (source_ == nullptr || !source_->Valid()) return false;

  uint8_t buf[kHeaderSize];
  if (source_->Read(0, buf, kHeaderSize) != kHeaderSize) return false;
  if (std::memcmp(buf, kMagic, sizeof(kMagic)) != 0) return false;

  header_ = PmTilesHeader{};
  header_.version = buf[7];
  if (header_.version != 3) return false;  // Only v3 is supported.

  header_.root_dir_offset = ReadU64LE(buf + 8);
  header_.root_dir_length = ReadU64LE(buf + 16);
  header_.metadata_offset = ReadU64LE(buf + 24);
  header_.metadata_length = ReadU64LE(buf + 32);
  header_.leaf_dirs_offset = ReadU64LE(buf + 40);
  header_.leaf_dirs_length = ReadU64LE(buf + 48);
  header_.tile_data_offset = ReadU64LE(buf + 56);
  header_.tile_data_length = ReadU64LE(buf + 64);
  header_.addressed_tiles_count = ReadU64LE(buf + 72);
  header_.tile_entries_count = ReadU64LE(buf + 80);
  header_.tile_contents_count = ReadU64LE(buf + 88);
  header_.clustered = buf[96] != 0;
  header_.internal_compression = static_cast<Compression>(buf[97]);
  header_.tile_compression = static_cast<Compression>(buf[98]);
  header_.tile_type = static_cast<TileType>(buf[99]);
  header_.min_zoom = buf[100];
  header_.max_zoom = buf[101];
  header_.min_lon_e7 = ReadI32LE(buf + 102);
  header_.min_lat_e7 = ReadI32LE(buf + 106);
  header_.max_lon_e7 = ReadI32LE(buf + 110);
  header_.max_lat_e7 = ReadI32LE(buf + 114);
  header_.center_zoom = buf[118];
  header_.center_lon_e7 = ReadI32LE(buf + 119);
  header_.center_lat_e7 = ReadI32LE(buf + 123);

  if (header_.root_dir_length > kMaxRootDirBytes) return false;

  if (!ReadDirectory(header_.root_dir_offset, header_.root_dir_length,
                      &root_dir_)) {
    return false;
  }

  open_ = true;
  return true;
}

bool PmTilesReader::ReadDirectory(uint64_t offset, uint64_t length,
                                   std::vector<DirEntry>* out) const {
  out->clear();
  if (length == 0 || length > kMaxDirectoryReadBytes) return false;

  std::vector<uint8_t> raw(length);
  if (source_->Read(offset, raw.data(), length) != length) return false;

  std::vector<uint8_t> decompressed;
  if (!DecompressPayload(header_.internal_compression, raw.data(), raw.size(),
                         kMaxDirectoryReadBytes, &decompressed)) {
    return false;
  }

  const uint8_t* data = decompressed.data();
  const size_t len = decompressed.size();
  size_t pos = 0;
  bool ok = false;

  const uint64_t num_entries = ReadVarint(data, len, &pos, &ok);
  if (!ok || num_entries > kMaxDirectoryReadBytes / 4) return false;

  out->resize(num_entries);

  uint64_t running_tile_id = 0;
  for (uint64_t i = 0; i < num_entries; ++i) {
    const uint64_t delta = ReadVarint(data, len, &pos, &ok);
    if (!ok) return false;
    running_tile_id += delta;
    (*out)[i].tile_id = running_tile_id;
  }
  for (uint64_t i = 0; i < num_entries; ++i) {
    const uint64_t run_length = ReadVarint(data, len, &pos, &ok);
    if (!ok) return false;
    (*out)[i].run_length = static_cast<uint32_t>(run_length);
  }
  for (uint64_t i = 0; i < num_entries; ++i) {
    const uint64_t entry_length = ReadVarint(data, len, &pos, &ok);
    if (!ok) return false;
    (*out)[i].length = static_cast<uint32_t>(entry_length);
  }
  uint64_t prev_offset = 0;
  uint64_t prev_length = 0;
  for (uint64_t i = 0; i < num_entries; ++i) {
    const uint64_t raw_offset = ReadVarint(data, len, &pos, &ok);
    if (!ok) return false;
    const uint64_t actual_offset =
        (raw_offset == 0) ? (prev_offset + prev_length) : (raw_offset - 1);
    (*out)[i].offset = actual_offset;
    prev_offset = actual_offset;
    prev_length = (*out)[i].length;
  }
  return true;
}

bool PmTilesReader::FindEntry(const std::vector<DirEntry>& dir,
                               uint64_t tile_id, DirEntry* found,
                               bool* is_leaf) {
  if (dir.empty()) return false;
  // Largest entry with entry.tile_id <= tile_id (std::upper_bound then
  // step back one), matching the PMTiles spec's lookup algorithm.
  auto it = std::upper_bound(
      dir.begin(), dir.end(), tile_id,
      [](uint64_t id, const DirEntry& e) { return id < e.tile_id; });
  if (it == dir.begin()) return false;
  --it;

  if (it->run_length == 0) {
    // Leaf-directory pointer: covers [tile_id, next_entry.tile_id).
    *found = *it;
    *is_leaf = true;
    return true;
  }
  if (tile_id >= it->tile_id && tile_id < it->tile_id + it->run_length) {
    *found = *it;
    *is_leaf = false;
    return true;
  }
  return false;
}

bool PmTilesReader::ReadMetadata(std::vector<uint8_t>* out) const {
  if (!open_) return false;
  if (header_.metadata_length == 0) {
    out->clear();
    return true;
  }
  if (header_.metadata_length > kMaxDirectoryReadBytes) return false;
  std::vector<uint8_t> raw(header_.metadata_length);
  if (source_->Read(header_.metadata_offset, raw.data(), raw.size()) !=
      raw.size()) {
    return false;
  }
  return DecompressPayload(header_.internal_compression, raw.data(), raw.size(),
                           kMaxDirectoryReadBytes, out);
}

const std::vector<PmTilesReader::DirEntry>* PmTilesReader::LeafDirectory(
    uint64_t offset, uint64_t length) const {
  for (const LeafCacheSlot& slot : leaf_cache_) {
    if (slot.valid && slot.offset == offset && slot.length == length) {
      return &slot.entries;
    }
  }
  LeafCacheSlot& slot = leaf_cache_[leaf_cache_next_];
  leaf_cache_next_ = (leaf_cache_next_ + 1) % kLeafCacheSlots;
  // Invalidate before reading: a failed read must not leave a stale slot
  // claiming to hold this directory.
  slot.valid = false;
  if (!ReadDirectory(offset, length, &slot.entries)) {
    slot.entries.clear();
    slot.entries.shrink_to_fit();
    return nullptr;
  }
  slot.offset = offset;
  slot.length = length;
  slot.valid = true;
  return &slot.entries;
}

bool PmTilesReader::GetTile(uint8_t z, uint32_t x, uint32_t y,
                             std::vector<uint8_t>* out) const {
  if (!open_) return false;
  const uint64_t tile_id = ZxyToTileId(z, x, y);

  DirEntry entry;
  bool is_leaf = false;
  if (!FindEntry(root_dir_, tile_id, &entry, &is_leaf)) return false;

  int leaf_hops = 0;
  while (is_leaf) {
    // Spec allows nested leaf directories; bound the hop count so a
    // corrupt/cyclic archive can't spin forever.
    if (++leaf_hops > 8) return false;
    const std::vector<DirEntry>* leaf =
        LeafDirectory(header_.leaf_dirs_offset + entry.offset, entry.length);
    if (leaf == nullptr) return false;
    // FindEntry copies what it needs into `entry`, so the cached vector is
    // not referenced past this call and a later miss may safely evict it.
    if (!FindEntry(*leaf, tile_id, &entry, &is_leaf)) return false;
  }

  out->resize(entry.length);
  if (entry.length == 0) return true;
  return source_->Read(header_.tile_data_offset + entry.offset, out->data(),
                        entry.length) == entry.length;
}

}  // namespace orcmap
