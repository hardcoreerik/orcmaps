#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace orcmap {

// Generic Mapbox Vector Tile (MVT) container decoder. Spec:
// github.com/mapbox/vector-tile-spec (an open specification -- this
// decoder implements it directly from the spec, not from any existing
// MVT library's source; see docs/DEPENDENCY_LEDGER.md).
//
// This decoder is deliberately SCHEMA-AGNOSTIC: it turns MVT bytes into
// layers/features/geometry/attributes exactly as encoded, with no opinion
// about what a layer named "water" or an attribute named "class" means.
// Mapping decoded features into the OrcMaps Feature / Geometry model is
// orcmap::TranslateMvtToFeatureTile() (include/orcmap/mvt_translate.hpp).
// Mapping those features to orcmap::FeatureKind is a separate, still
// experimental step -- see docs/FORMAT_DECISION.md "Deferred: tile content
// schema". Keeping this decoder schema-agnostic means that decision can
// still go either way without rewriting this file.
//
// Bounded-memory by construction: decoding one tile allocates only what
// that tile's own layers/features/geometry require (typically a few KB to
// tens of KB for a real tile), and never touches anything outside the
// caller-supplied byte range -- there is no separate "whole archive" state
// here, unlike PmTilesReader (which this decoder has no dependency on;
// PmTilesReader hands over already-extracted, already-decompressed tile
// bytes -- see orcmap::PmTilesReader::GetTile()).

enum class MvtGeomType : uint8_t {
  kUnknown = 0,
  kPoint = 1,
  kLineString = 2,
  kPolygon = 3,
};

// One decoded attribute value. MVT's four wire representations (string,
// float, double, one of four integer/bool encodings) collapse to this
// set: both float_value and double_value become `double`; int_value and
// sint_value (already zigzag-decoded) become `int64_t`; uint_value stays
// `uint64_t`; bool_value stays `bool`. `std::monostate` represents "not
// present" (should not normally appear in a decoded feature's attributes,
// only as MvtLayer::values[]' notional "no value" case, which MVT's own
// spec doesn't actually allow -- present for variant completeness).
using MvtValue = std::variant<std::monostate, std::string, double, int64_t,
                               uint64_t, bool>;

struct MvtPoint {
  int32_t x = 0;  // Tile-local coordinates, [0, layer.extent) nominally
  int32_t y = 0;  // (MVT allows slight overflow for features crossing a
                  // tile's buffer zone; this decoder does not clamp it).
};

// One geometry ring/path: for kPoint, one point per MvtPoint entry (MVT
// allows multi-point features); for kLineString, a connected sequence of
// points; for kPolygon, one ring (the decoder does not compute which
// rings are outer/inner -- see the file header comment).
using MvtRing = std::vector<MvtPoint>;

struct MvtFeature {
  uint64_t id = 0;
  MvtGeomType geom_type = MvtGeomType::kUnknown;
  std::vector<MvtRing> geometry;
  // Parallel arrays, not a map: attribute order as encoded is preserved,
  // and small linear scans are cheaper than a map on embedded hardware
  // for the handful of attributes a real feature carries.
  std::vector<std::string> attribute_keys;
  std::vector<MvtValue> attribute_values;
};

struct MvtLayer {
  std::string name;
  uint32_t version = 1;
  uint32_t extent = 4096;
  std::vector<MvtFeature> features;
};

struct MvtTile {
  std::vector<MvtLayer> layers;
};

// Optional decode policy. `include_layer` is a non-owning function pointer
// (no std::function). Null include_layer decodes every layer (legacy).
// The callback must not assume OpenMapTiles names; callers supply policy.
// Skipped layers are omitted from `out` (no partial features). A skipped
// layer is only checked for top-level protobuf framing and a name; its
// feature/key/value payloads are not semantically decoded.
struct MvtLayer;
struct MvtFeature;

// STREAMING decode. When `feature_sink` is set, decoding does NOT build an
// MvtTile: each feature is handed to the sink as soon as it is parsed, into
// a single reused MvtFeature, and then discarded. `out` is left empty.
//
// This exists because materialising a tile is what makes OrcMaps unusable on
// a board without PSRAM. Measured on real packs, a tile whose inflated bytes
// are 16-111 KiB materialises into an MvtTile plus a FeatureTile totalling
// 6-9x that -- up to 1,011 KiB for one Oregon z7 tile, against roughly
// 150 KiB of usable DRAM on an ESP32-3248S035. Streaming reduces the peak to
// the inflated bytes plus one feature.
//
// The sink receives the owning layer (name/extent/version populated, its
// `features` vector deliberately empty) so the existing per-feature
// translation applies unchanged. Nothing passed to the sink outlives the
// call: copy what you need. Returning false from the sink aborts the decode
// and makes the decode call return false.
using MvtFeatureSink = bool (*)(const MvtLayer& layer,
                                const MvtFeature& feature, void* ctx);

struct MvtDecodeOptions {
  bool (*include_layer)(const char* name, size_t name_len, void* ctx) = nullptr;
  void* include_layer_ctx = nullptr;
  MvtFeatureSink feature_sink = nullptr;
  void* feature_sink_ctx = nullptr;
};

// Decodes one MVT tile's raw bytes (after DecompressPayload if the
// archive stored gzip/etc.). Returns false on any
// structural problem (truncated data, invalid varint, malformed geometry
// command stream, tag index out of range) -- never throws, never reads
// past `length`.
bool DecodeMvtTile(const uint8_t* data, size_t length, MvtTile* out);
bool DecodeMvtTile(const uint8_t* data, size_t length,
                   const MvtDecodeOptions& options, MvtTile* out);

}  // namespace orcmap
