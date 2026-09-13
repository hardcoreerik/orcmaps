#pragma once

#include <cstdint>
#include <string_view>

#include "orcmap/style.hpp"

namespace orcmap {

// Bump this whenever tile decode or draw logic changes in a way that
// changes rendered pixel output (not on every commit -- only when a
// rendered tile from before the change would look different after it).
// Part of the rendered-tile cache key so a renderer code change can't
// silently serve stale-looking cached tiles. See docs/STYLING.md "rendered
// tile cache interaction".
inline constexpr uint32_t kRendererVersion = 1;

// FNV-1a, used only to fold small identifier strings (pack id, style id)
// into a fixed-width cache-key field -- not a cryptographic hash and not
// used anywhere integrity matters (that's SHA-256 over full pack bytes,
// per docs/DATA_AND_LICENSING.md).
constexpr uint32_t Fnv1aHash(std::string_view s) {
  uint32_t hash = 2166136261u;
  for (unsigned char c : s) {
    hash ^= c;
    hash *= 16777619u;
  }
  return hash;
}

// Identifies one rendered tile uniquely enough that changing any input
// that affects its pixels changes the key. If OrcMaps caches pre-rendered
// (e.g. RGB565) tiles on SD -- see docs/FORMAT_DECISION.md "Decided:
// container format" -- every cache read/write must go through this key,
// not just (pack, z, x, y): a style switch or a renderer update must never
// let a tile rendered under the old style/renderer silently reappear.
// See docs/STYLING.md "rendered tile cache interaction" for the incident
// this is designed to prevent.
struct RenderedTileCacheKey {
  uint32_t pack_id_hash = 0;  // Fnv1aHash(pack.id) -- see docs/PACK_FORMAT.md.
  uint8_t z = 0;
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t style_id_hash = 0;   // Fnv1aHash(style.id)
  uint32_t style_version = 0;   // style.version
  uint32_t renderer_version = kRendererVersion;

  static RenderedTileCacheKey Make(std::string_view pack_id, uint8_t z,
                                    uint32_t x, uint32_t y,
                                    const MapStyle& style) {
    RenderedTileCacheKey key;
    key.pack_id_hash = Fnv1aHash(pack_id);
    key.z = z;
    key.x = x;
    key.y = y;
    key.style_id_hash = Fnv1aHash(style.id);
    key.style_version = style.version;
    key.renderer_version = kRendererVersion;
    return key;
  }

  bool operator==(const RenderedTileCacheKey& other) const {
    return pack_id_hash == other.pack_id_hash && z == other.z &&
           x == other.x && y == other.y &&
           style_id_hash == other.style_id_hash &&
           style_version == other.style_version &&
           renderer_version == other.renderer_version;
  }
  bool operator!=(const RenderedTileCacheKey& other) const {
    return !(*this == other);
  }
};

}  // namespace orcmap
