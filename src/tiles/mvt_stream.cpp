#include "orcmap/mvt_stream.hpp"

#include <utility>

#include "mvt_internal.hpp"

namespace orcmap {
namespace {

// Skips one protobuf field on a forward-only stream.
bool SkipField(InflatingByteStream* in, uint32_t wire) {
  switch (wire) {
    case 0: {
      uint64_t ignored = 0;
      return in->ReadVarint(&ignored);
    }
    case 1:
      return in->Skip(8);
    case 2: {
      uint64_t len = 0;
      if (!in->ReadVarint(&len)) return false;
      return in->Skip(len);
    }
    case 5:
      return in->Skip(4);
    default:
      return false;  // groups (3/4) are not used by MVT
  }
}

// Reads a length-delimited field's payload into `buf`, bounded so hostile
// input cannot force an unbounded allocation.
bool ReadDelimited(InflatingByteStream* in, uint64_t len,
                   std::vector<uint8_t>* buf) {
  if (len > kMaxStreamedFeatureBytes) return false;
  buf->resize(static_cast<size_t>(len));
  if (len == 0) return true;
  return in->Read(buf->data(), static_cast<size_t>(len));
}

// PASS 1 over one layer: name, extent, version, keys, values. Feature
// payloads are skipped -- their attribute indices cannot be resolved yet,
// because MVT emits features before the tables they reference.
bool ReadLayerTables(InflatingByteStream* in, uint64_t layer_len,
                     const MvtDecodeOptions& options,
                     std::vector<uint8_t>* staging,
                     MvtStreamScratch::LayerTables* out) {
  const uint64_t end = in->consumed() + layer_len;
  out->name.clear();
  out->extent = 4096;
  out->version = 1;
  out->included = false;
  out->keys.clear();
  out->values.clear();
  bool has_name = false;

  while (in->consumed() < end) {
    uint64_t tag = 0;
    if (!in->ReadVarint(&tag)) return false;
    const uint32_t field = static_cast<uint32_t>(tag >> 3);
    const uint32_t wire = static_cast<uint32_t>(tag & 7);
    if (field == 1 && wire == 2) {
      uint64_t len = 0;
      if (!in->ReadVarint(&len)) return false;
      if (!ReadDelimited(in, len, staging)) return false;
      out->name.assign(reinterpret_cast<const char*>(staging->data()),
                       staging->size());
      has_name = true;
    } else if (field == 3 && wire == 2) {
      uint64_t len = 0;
      if (!in->ReadVarint(&len)) return false;
      if (!ReadDelimited(in, len, staging)) return false;
      out->keys.emplace_back(reinterpret_cast<const char*>(staging->data()),
                             staging->size());
    } else if (field == 4 && wire == 2) {
      uint64_t len = 0;
      if (!in->ReadVarint(&len)) return false;
      if (!ReadDelimited(in, len, staging)) return false;
      MvtValue value;
      if (!internal::DecodeValue(staging->data(), staging->size(), &value)) {
        return false;
      }
      out->values.push_back(std::move(value));
    } else if (field == 5 && wire == 0) {
      uint64_t v = 0;
      if (!in->ReadVarint(&v)) return false;
      out->extent = static_cast<uint32_t>(v);
    } else if (field == 15 && wire == 0) {
      uint64_t v = 0;
      if (!in->ReadVarint(&v)) return false;
      out->version = static_cast<uint32_t>(v);
    } else if (!SkipField(in, wire)) {
      return false;
    }
  }
  if (in->consumed() != end || !has_name) return false;

  out->included = options.include_layer == nullptr ||
                  options.include_layer(out->name.c_str(), out->name.size(),
                                        options.include_layer_ctx);
  if (!out->included) {
    // Drop an excluded layer's tables straight away: on a constrained board
    // the memory matters more than keeping the record tidy.
    out->keys.clear();
    out->keys.shrink_to_fit();
    out->values.clear();
    out->values.shrink_to_fit();
  }
  return true;
}

// PASS 2 over one layer: decode each feature and hand it to the sink. One
// feature's bytes and one decoded feature exist at a time.
bool StreamLayerFeatures(InflatingByteStream* in, uint64_t layer_len,
                         const MvtDecodeOptions& options,
                         const MvtStreamScratch::LayerTables& tables,
                         MvtStreamScratch* scratch) {
  const uint64_t end = in->consumed() + layer_len;
  if (!tables.included) {
    return in->Skip(end - in->consumed()) && in->consumed() == end;
  }

  scratch->layer.name = tables.name;
  scratch->layer.extent = tables.extent;
  scratch->layer.version = tables.version;
  scratch->layer.features.clear();

  while (in->consumed() < end) {
    uint64_t tag = 0;
    if (!in->ReadVarint(&tag)) return false;
    const uint32_t field = static_cast<uint32_t>(tag >> 3);
    const uint32_t wire = static_cast<uint32_t>(tag & 7);
    if (field == 2 && wire == 2) {
      uint64_t len = 0;
      if (!in->ReadVarint(&len)) return false;
      if (!ReadDelimited(in, len, &scratch->feature_bytes)) return false;

      // Clear, keep capacity: one feature object serves the whole tile.
      MvtFeature& feature = scratch->feature;
      feature.id = 0;
      feature.geom_type = MvtGeomType::kUnknown;
      for (MvtRing& ring : feature.geometry) ring.clear();
      feature.geometry.clear();
      feature.attribute_keys.clear();
      feature.attribute_values.clear();
      if (!internal::DecodeFeature(scratch->feature_bytes.data(),
                                   scratch->feature_bytes.size(), tables.keys,
                                   tables.values, &feature)) {
        return false;
      }
      if (!options.feature_sink(scratch->layer, feature,
                                options.feature_sink_ctx)) {
        return false;
      }
    } else if (!SkipField(in, wire)) {
      return false;
    }
  }
  return in->consumed() == end;
}

}  // namespace

bool StreamMvtTile(Compression compression, CompressedChunkReader reader,
                   void* ctx, uint64_t input_offset, size_t input_size,
                   const MvtDecodeOptions& options,
                   MvtStreamScratch* scratch) {
  if (scratch == nullptr || options.feature_sink == nullptr) return false;
  scratch->layers.clear();

  // PASS 1: tables only.
  if (!scratch->stream.Begin(compression, reader, ctx, input_offset,
                             input_size)) {
    return false;
  }
  std::vector<uint8_t> staging;
  while (!scratch->stream.AtEnd()) {
    uint64_t tag = 0;
    if (!scratch->stream.ReadVarint(&tag)) return false;
    const uint32_t field = static_cast<uint32_t>(tag >> 3);
    const uint32_t wire = static_cast<uint32_t>(tag & 7);
    if (field == 3 && wire == 2) {  // Tile.layers
      uint64_t len = 0;
      if (!scratch->stream.ReadVarint(&len)) return false;
      scratch->layers.emplace_back();
      if (!ReadLayerTables(&scratch->stream, len, options, &staging,
                           &scratch->layers.back())) {
        return false;
      }
    } else if (!SkipField(&scratch->stream, wire)) {
      return false;
    }
  }

  // PASS 2: features, resolved against pass 1's tables. Layer order is
  // identical because this is the same byte sequence re-inflated.
  if (!scratch->stream.Begin(compression, reader, ctx, input_offset,
                             input_size)) {
    return false;
  }
  size_t layer_index = 0;
  while (!scratch->stream.AtEnd()) {
    uint64_t tag = 0;
    if (!scratch->stream.ReadVarint(&tag)) return false;
    const uint32_t field = static_cast<uint32_t>(tag >> 3);
    const uint32_t wire = static_cast<uint32_t>(tag & 7);
    if (field == 3 && wire == 2) {
      uint64_t len = 0;
      if (!scratch->stream.ReadVarint(&len)) return false;
      if (layer_index >= scratch->layers.size()) return false;
      if (!StreamLayerFeatures(&scratch->stream, len, options,
                               scratch->layers[layer_index], scratch)) {
        return false;
      }
      ++layer_index;
    } else if (!SkipField(&scratch->stream, wire)) {
      return false;
    }
  }
  // Both passes must agree on the layer count, or the tile changed under us.
  return layer_index == scratch->layers.size();
}

}  // namespace orcmap
