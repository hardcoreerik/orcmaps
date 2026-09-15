#include "orcmap/mvt.hpp"

#include "mvt_internal.hpp"

#include <cstring>

namespace orcmap {

namespace {

// --- Minimal protobuf wire-format reader -------------------------------
//
// MVT is protobuf-encoded, but OrcMaps deliberately does not depend on a
// general-purpose protobuf library (see docs/DEPENDENCY_LEDGER.md
// "candidates under evaluation" -- a full protobuf runtime is a cost this
// project chose to avoid for an embedded target when only a handful of
// fixed, known message shapes ever need reading). This reader implements
// just enough of the wire format (varint/LEN/32-bit/64-bit fields, tag
// parsing, unknown-field skipping) to walk MVT's four message types
// (Tile, Layer, Feature, Value), all defined in the open MVT spec
// (github.com/mapbox/vector-tile-spec). It is not a general protobuf
// parser and must not be used as one.

struct Reader {
  const uint8_t* data;
  size_t length;
  size_t pos = 0;

  bool AtEnd() const { return pos >= length; }

  bool ReadVarint(uint64_t* out) {
    uint64_t result = 0;
    int shift = 0;
    while (pos < length) {
      uint8_t byte = data[pos++];
      result |= static_cast<uint64_t>(byte & 0x7f) << shift;
      if ((byte & 0x80) == 0) {
        *out = result;
        return true;
      }
      shift += 7;
      if (shift > 63) return false;
    }
    return false;
  }

  bool ReadTag(uint32_t* field_number, uint32_t* wire_type) {
    uint64_t tag = 0;
    if (!ReadVarint(&tag)) return false;
    *field_number = static_cast<uint32_t>(tag >> 3);
    *wire_type = static_cast<uint32_t>(tag & 0x7);
    return true;
  }

  // Returns a view [start, start+len) into `data` for a LEN-type field,
  // advancing `pos` past it. Never copies.
  bool ReadLengthDelimited(const uint8_t** out_data, size_t* out_len) {
    uint64_t len = 0;
    if (!ReadVarint(&len)) return false;
    if (len > length - pos) return false;  // Truncated -- would read past end.
    *out_data = data + pos;
    *out_len = static_cast<size_t>(len);
    pos += static_cast<size_t>(len);
    return true;
  }

  bool ReadFixed32(uint32_t* out) {
    if (length - pos < 4) return false;
    std::memcpy(out, data + pos, 4);
    pos += 4;
    return true;
  }

  bool ReadFixed64(uint64_t* out) {
    if (length - pos < 8) return false;
    std::memcpy(out, data + pos, 8);
    pos += 8;
    return true;
  }

  bool SkipField(uint32_t wire_type) {
    switch (wire_type) {
      case 0: {
        uint64_t discard;
        return ReadVarint(&discard);
      }
      case 1:
        if (length - pos < 8) return false;
        pos += 8;
        return true;
      case 2: {
        const uint8_t* discard_data;
        size_t discard_len;
        return ReadLengthDelimited(&discard_data, &discard_len);
      }
      case 5:
        if (length - pos < 4) return false;
        pos += 4;
        return true;
      default:
        return false;  // Wire types 3/4 (deprecated groups) never appear in MVT.
    }
  }
};

int64_t ZigZagDecode(uint64_t n) {
  return static_cast<int64_t>(n >> 1) ^ -static_cast<int64_t>(n & 1);
}

// --- Value (Layer.values[] entry) ---------------------------------------

}  // namespace

namespace internal {

bool DecodeValue(const uint8_t* data, size_t length, MvtValue* out) {
  Reader r{data, length};
  while (!r.AtEnd()) {
    uint32_t field_number, wire_type;
    if (!r.ReadTag(&field_number, &wire_type)) return false;
    switch (field_number) {
      case 1: {  // string_value
        const uint8_t* str_data;
        size_t str_len;
        if (wire_type != 2 || !r.ReadLengthDelimited(&str_data, &str_len)) return false;
        *out = std::string(reinterpret_cast<const char*>(str_data), str_len);
        break;
      }
      case 2: {  // float_value
        uint32_t bits;
        if (wire_type != 5 || !r.ReadFixed32(&bits)) return false;
        float f;
        std::memcpy(&f, &bits, 4);
        *out = static_cast<double>(f);
        break;
      }
      case 3: {  // double_value
        uint64_t bits;
        if (wire_type != 1 || !r.ReadFixed64(&bits)) return false;
        double d;
        std::memcpy(&d, &bits, 8);
        *out = d;
        break;
      }
      case 4: {  // int_value
        uint64_t v;
        if (wire_type != 0 || !r.ReadVarint(&v)) return false;
        *out = static_cast<int64_t>(v);
        break;
      }
      case 5: {  // uint_value
        uint64_t v;
        if (wire_type != 0 || !r.ReadVarint(&v)) return false;
        *out = v;
        break;
      }
      case 6: {  // sint_value (zigzag)
        uint64_t v;
        if (wire_type != 0 || !r.ReadVarint(&v)) return false;
        *out = ZigZagDecode(v);
        break;
      }
      case 7: {  // bool_value
        uint64_t v;
        if (wire_type != 0 || !r.ReadVarint(&v)) return false;
        *out = v != 0;
        break;
      }
      default:
        if (!r.SkipField(wire_type)) return false;
    }
  }
  return true;
}

// --- Geometry command stream ---------------------------------------------

// Templated on the destination ring container so ONE implementation serves
// both the MVT-shaped output (std::vector<MvtRing> of MvtPoint) and the
// OrcMaps-shaped output (std::vector<Path> of Point). The two point types are
// layout-identical two-int32 structs; keeping a single parser is what stops
// the streaming path and the buffered path drifting apart.
// Templated on the destination ring container so ONE implementation serves
// both the MVT-shaped output (std::vector<MvtRing> of MvtPoint) and the
// OrcMaps-shaped output (std::vector<Path> of Point). The two point types are
// layout-identical two-int32 structs; keeping a single parser is what stops
// the streaming path and the buffered path drifting apart.
//
// Rings are REUSED, not cleared and re-pushed: a caller decoding a tile's
// features one at a time keeps each ring's point buffer, so a dense tile
// does not churn one allocation per ring per feature. `out` is resized down
// to the ring count this geometry actually used.
template <typename RingVector>
bool DecodeGeometryInto(const std::vector<uint32_t>& commands,
                        MvtGeomType geom_type, RingVector* out) {
  using RingType = typename RingVector::value_type;
  using PointType = typename RingType::value_type;

  size_t used = 0;
  const auto open_ring = [&]() -> RingType& {
    if (used < out->size()) {
      (*out)[used].clear();
    } else {
      out->emplace_back();
    }
    return (*out)[used++];
  };
  const auto current_ring = [&]() -> RingType* {
    return used == 0 ? nullptr : &(*out)[used - 1];
  };

  int32_t cx = 0, cy = 0;
  size_t i = 0;
  while (i < commands.size()) {
    const uint32_t cmd_int = commands[i++];
    const uint32_t cmd_id = cmd_int & 0x7;
    const uint32_t count = cmd_int >> 3;

    if (cmd_id == 1) {  // MoveTo
      if (geom_type == MvtGeomType::kPoint) {
        // Multipoint: every point joins one ring.
        if (used == 0) open_ring();
        RingType& ring = *current_ring();
        for (uint32_t k = 0; k < count; ++k) {
          if (commands.size() - i < 2) return false;
          cx += static_cast<int32_t>(ZigZagDecode(commands[i++]));
          cy += static_cast<int32_t>(ZigZagDecode(commands[i++]));
          ring.push_back(PointType{cx, cy});
        }
      } else {
        for (uint32_t k = 0; k < count; ++k) {
          if (commands.size() - i < 2) return false;
          cx += static_cast<int32_t>(ZigZagDecode(commands[i++]));
          cy += static_cast<int32_t>(ZigZagDecode(commands[i++]));
          open_ring().push_back(PointType{cx, cy});
        }
      }
    } else if (cmd_id == 2) {  // LineTo
      RingType* ring = current_ring();
      if (ring == nullptr) return false;
      for (uint32_t k = 0; k < count; ++k) {
        if (commands.size() - i < 2) return false;
        cx += static_cast<int32_t>(ZigZagDecode(commands[i++]));
        cy += static_cast<int32_t>(ZigZagDecode(commands[i++]));
        ring->push_back(PointType{cx, cy});
      }
    } else if (cmd_id == 7) {  // ClosePath
      if (used == 0 || count != 1) return false;
      // No parameters; ring closure is implicit (first/last point join),
      // matching the MVT spec -- this decoder does not duplicate the
      // first point onto the end of the ring.
    } else {
      return false;  // Unknown command id.
    }
  }
  out->resize(used);
  return true;
}

// --- Feature ---------------------------------------------------------------

// Walks one Feature message's fields into `id`, `type` and the two packed
// arrays. Shared by both destinations so the wire handling exists once.
bool ParseFeatureFields(const uint8_t* data, size_t length, uint64_t* id,
                        MvtGeomType* type, std::vector<uint32_t>* tags_out,
                        std::vector<uint32_t>* geometry_out) {
  Reader r{data, length};
  std::vector<uint32_t>& tags = *tags_out;
  std::vector<uint32_t>& geometry_commands = *geometry_out;
  tags.clear();
  geometry_commands.clear();
  *id = 0;
  *type = MvtGeomType::kUnknown;

  while (!r.AtEnd()) {
    uint32_t field_number, wire_type;
    if (!r.ReadTag(&field_number, &wire_type)) return false;
    switch (field_number) {
      case 1: {  // id
        uint64_t v;
        if (wire_type != 0 || !r.ReadVarint(&v)) return false;
        *id = v;
        break;
      }
      case 2: {  // tags (packed or unpacked repeated uint32)
        if (wire_type == 2) {
          const uint8_t* packed_data;
          size_t packed_len;
          if (!r.ReadLengthDelimited(&packed_data, &packed_len)) return false;
          Reader packed{packed_data, packed_len};
          while (!packed.AtEnd()) {
            uint64_t v;
            if (!packed.ReadVarint(&v)) return false;
            tags.push_back(static_cast<uint32_t>(v));
          }
        } else if (wire_type == 0) {
          uint64_t v;
          if (!r.ReadVarint(&v)) return false;
          tags.push_back(static_cast<uint32_t>(v));
        } else {
          return false;
        }
        break;
      }
      case 3: {  // type
        uint64_t v;
        if (wire_type != 0 || !r.ReadVarint(&v)) return false;
        if (v > 3) return false;
        *type = static_cast<MvtGeomType>(v);
        break;
      }
      case 4: {  // geometry (packed or unpacked repeated uint32)
        if (wire_type == 2) {
          const uint8_t* packed_data;
          size_t packed_len;
          if (!r.ReadLengthDelimited(&packed_data, &packed_len)) return false;
          Reader packed{packed_data, packed_len};
          while (!packed.AtEnd()) {
            uint64_t v;
            if (!packed.ReadVarint(&v)) return false;
            geometry_commands.push_back(static_cast<uint32_t>(v));
          }
        } else if (wire_type == 0) {
          uint64_t v;
          if (!r.ReadVarint(&v)) return false;
          geometry_commands.push_back(static_cast<uint32_t>(v));
        } else {
          return false;
        }
        break;
      }
      default:
        if (!r.SkipField(wire_type)) return false;
    }
  }

  return tags.size() % 2 == 0;
}

bool DecodeFeature(const uint8_t* data, size_t length,
                   const std::vector<std::string>& layer_keys,
                   const std::vector<MvtValue>& layer_values, MvtFeature* out) {
  std::vector<uint32_t> tags;
  std::vector<uint32_t> geometry_commands;
  if (!ParseFeatureFields(data, length, &out->id, &out->geom_type, &tags,
                          &geometry_commands)) {
    return false;
  }
  for (size_t t = 0; t < tags.size(); t += 2) {
    const uint32_t key_index = tags[t];
    const uint32_t value_index = tags[t + 1];
    if (key_index >= layer_keys.size() || value_index >= layer_values.size()) {
      return false;
    }
    out->attribute_keys.push_back(layer_keys[key_index]);
    out->attribute_values.push_back(layer_values[value_index]);
  }
  return DecodeGeometryInto(geometry_commands, out->geom_type, &out->geometry);
}

bool DecodeFeatureAsFeature(const uint8_t* data, size_t length,
                            const std::vector<std::string>& layer_keys,
                            const std::vector<MvtValue>& layer_values,
                            const MvtLayer& layer,
                            FeatureParseScratch* scratch, Feature* out) {
  if (scratch == nullptr || out == nullptr) return false;
  MvtGeomType type = MvtGeomType::kUnknown;
  if (!ParseFeatureFields(data, length, &out->id, &type, &scratch->tags,
                          &scratch->geometry)) {
    return false;
  }

  // Clear contents, keep buffers: one Feature serves every feature in a tile.
  out->kind_assigned = false;
  out->kind = FeatureKind::kBackground;
  out->extent = layer.extent == 0 ? 4096 : layer.extent;
  out->layer = layer.name;
  out->property_keys.clear();
  out->property_values.clear();
  for (size_t t = 0; t < scratch->tags.size(); t += 2) {
    const uint32_t key_index = scratch->tags[t];
    const uint32_t value_index = scratch->tags[t + 1];
    if (key_index >= layer_keys.size() || value_index >= layer_values.size()) {
      return false;
    }
    out->property_keys.push_back(layer_keys[key_index]);
    out->property_values.push_back(layer_values[value_index]);
  }

  switch (type) {
    case MvtGeomType::kPoint: out->geometry.type = GeomType::kPoint; break;
    case MvtGeomType::kLineString:
      out->geometry.type = GeomType::kLineString;
      break;
    case MvtGeomType::kPolygon: out->geometry.type = GeomType::kPolygon; break;
    case MvtGeomType::kUnknown:
    default: out->geometry.type = GeomType::kUnknown; break;
  }
  // Rings are reused in place by the decoder, so the Paths keep their point
  // buffers between features.
  return DecodeGeometryInto(scratch->geometry, type, &out->geometry.paths);
}

// --- Layer -------------------------------------------------------------

// Name + top-level framing only. Nested key/value/feature payloads are
// skipped as length-delimited blobs (no MvtValue / MvtFeature allocation).
}  // namespace internal

namespace {

bool InspectLayerName(const uint8_t* data, size_t length, std::string* name) {
  Reader r{data, length};
  bool has_name = false;
  while (!r.AtEnd()) {
    uint32_t field_number, wire_type;
    if (!r.ReadTag(&field_number, &wire_type)) return false;
    if (field_number == 1) {
      const uint8_t* str_data;
      size_t str_len;
      if (wire_type != 2 || !r.ReadLengthDelimited(&str_data, &str_len)) {
        return false;
      }
      name->assign(reinterpret_cast<const char*>(str_data), str_len);
      has_name = true;
    } else {
      if (!r.SkipField(wire_type)) return false;
    }
  }
  return has_name;
}

bool DecodeLayer(const uint8_t* data, size_t length,
                 const MvtDecodeOptions& options, MvtLayer* out,
                 bool* keep) {
  Reader r{data, length};
  std::vector<std::string> keys;
  std::vector<MvtValue> values;
  std::vector<std::pair<const uint8_t*, size_t>> feature_blobs;
  bool has_name = false;

  while (!r.AtEnd()) {
    uint32_t field_number, wire_type;
    if (!r.ReadTag(&field_number, &wire_type)) return false;
    switch (field_number) {
      case 1: {  // name
        const uint8_t* str_data;
        size_t str_len;
        if (wire_type != 2 || !r.ReadLengthDelimited(&str_data, &str_len)) return false;
        out->name.assign(reinterpret_cast<const char*>(str_data), str_len);
        has_name = true;
        break;
      }
      case 2: {  // features -- deferred until keys/values are fully read.
        const uint8_t* feature_data;
        size_t feature_len;
        if (wire_type != 2 || !r.ReadLengthDelimited(&feature_data, &feature_len)) {
          return false;
        }
        feature_blobs.emplace_back(feature_data, feature_len);
        break;
      }
      case 3: {  // keys
        const uint8_t* str_data;
        size_t str_len;
        if (wire_type != 2 || !r.ReadLengthDelimited(&str_data, &str_len)) return false;
        keys.emplace_back(reinterpret_cast<const char*>(str_data), str_len);
        break;
      }
      case 4: {  // values
        const uint8_t* value_data;
        size_t value_len;
        if (wire_type != 2 || !r.ReadLengthDelimited(&value_data, &value_len)) return false;
        MvtValue value;
        if (!internal::DecodeValue(value_data, value_len, &value)) return false;
        values.push_back(std::move(value));
        break;
      }
      case 5: {  // extent
        uint64_t v;
        if (wire_type != 0 || !r.ReadVarint(&v)) return false;
        out->extent = static_cast<uint32_t>(v);
        break;
      }
      case 15: {  // version
        uint64_t v;
        if (wire_type != 0 || !r.ReadVarint(&v)) return false;
        out->version = static_cast<uint32_t>(v);
        break;
      }
      default:
        if (!r.SkipField(wire_type)) return false;
    }
  }
  if (!has_name) return false;
  if (options.include_layer != nullptr &&
      !options.include_layer(out->name.c_str(), out->name.size(),
                             options.include_layer_ctx)) {
    *keep = false;
    return true;
  }
  *keep = true;

  if (options.feature_sink != nullptr) {
    // Streaming: one reused feature, handed over and discarded. Clearing
    // rather than reconstructing keeps the vectors' capacity, so a tile with
    // hundreds of features does not re-allocate hundreds of times.
    MvtFeature feature;
    for (const auto& [feature_data, feature_len] : feature_blobs) {
      feature.id = 0;
      feature.geom_type = MvtGeomType::kUnknown;
      for (MvtRing& ring : feature.geometry) ring.clear();
      feature.geometry.clear();
      feature.attribute_keys.clear();
      feature.attribute_values.clear();
      if (!internal::DecodeFeature(feature_data, feature_len, keys, values,
                                   &feature)) {
        return false;
      }
      if (!options.feature_sink(*out, feature, options.feature_sink_ctx)) {
        return false;
      }
    }
    return true;
  }

  out->features.reserve(feature_blobs.size());
  for (const auto& [feature_data, feature_len] : feature_blobs) {
    MvtFeature feature;
    if (!internal::DecodeFeature(feature_data, feature_len, keys, values,
                                 &feature)) {
      return false;
    }
    out->features.push_back(std::move(feature));
  }
  return true;
}

}  // namespace

bool DecodeMvtTile(const uint8_t* data, size_t length, MvtTile* out) {
  return DecodeMvtTile(data, length, MvtDecodeOptions{}, out);
}

bool DecodeMvtTile(const uint8_t* data, size_t length,
                   const MvtDecodeOptions& options, MvtTile* out) {
  if (data == nullptr || out == nullptr) return false;
  out->layers.clear();

  Reader r{data, length};
  while (!r.AtEnd()) {
    uint32_t field_number, wire_type;
    if (!r.ReadTag(&field_number, &wire_type)) return false;
    if (field_number == 3) {  // Tile.layers
      const uint8_t* layer_data;
      size_t layer_len;
      if (wire_type != 2 || !r.ReadLengthDelimited(&layer_data, &layer_len)) {
        return false;
      }
      if (options.include_layer != nullptr) {
        std::string layer_name;
        if (!InspectLayerName(layer_data, layer_len, &layer_name)) return false;
        if (!options.include_layer(layer_name.c_str(), layer_name.size(),
                                   options.include_layer_ctx)) {
          continue;
        }
      }
      MvtLayer layer;
      bool keep = true;
      // include_layer was already applied above; forward only the sink so a
      // streaming caller still receives this layer's features.
      MvtDecodeOptions inner;
      inner.feature_sink = options.feature_sink;
      inner.feature_sink_ctx = options.feature_sink_ctx;
      if (!DecodeLayer(layer_data, layer_len, inner, &layer, &keep)) {
        return false;
      }
      if (keep && options.feature_sink == nullptr) {
        out->layers.push_back(std::move(layer));
      }
    } else {
      if (!r.SkipField(wire_type)) return false;
    }
  }
  return true;
}

}  // namespace orcmap
