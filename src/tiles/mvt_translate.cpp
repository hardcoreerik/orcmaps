#include "orcmap/mvt_translate.hpp"

#include <utility>

namespace orcmap {

namespace {

GeomType MapGeomType(MvtGeomType type) {
  switch (type) {
    case MvtGeomType::kPoint:
      return GeomType::kPoint;
    case MvtGeomType::kLineString:
      return GeomType::kLineString;
    case MvtGeomType::kPolygon:
      return GeomType::kPolygon;
    case MvtGeomType::kUnknown:
    default:
      return GeomType::kUnknown;
  }
}

void CopyFeatureInto(const MvtLayer& layer, const MvtFeature& in,
                     Feature* out_ptr) {
  Feature& out = *out_ptr;
  // Clear contents, keep buffers: a streaming caller reuses one Feature for
  // every feature in a tile, and retaining capacity is the whole point.
  out.property_keys.clear();
  out.property_values.clear();
  out.id = in.id;
  out.kind_assigned = false;
  out.extent = layer.extent == 0 ? 4096 : layer.extent;
  out.layer = layer.name;
  out.geometry.type = MapGeomType(in.geom_type);
  // Resize rather than clear-and-push: retained Path elements keep their
  // point buffers, so a tile's features reuse the same allocations instead
  // of churning one vector per ring per feature.
  out.geometry.paths.resize(in.geometry.size());
  for (size_t i = 0; i < in.geometry.size(); ++i) {
    const MvtRing& ring = in.geometry[i];
    Path& path = out.geometry.paths[i];
    path.clear();
    path.reserve(ring.size());
    for (const MvtPoint& p : ring) {
      path.push_back(Point{p.x, p.y});
    }
  }
  out.property_keys = in.attribute_keys;
  out.property_values.reserve(in.attribute_values.size());
  for (const MvtValue& value : in.attribute_values) {
    out.property_values.push_back(value);
  }
}

Feature CopyFeature(const MvtLayer& layer, const MvtFeature& in) {
  Feature out;
  CopyFeatureInto(layer, in, &out);
  return out;
}

}  // namespace

bool TranslateMvtFeature(const MvtLayer& layer, const MvtFeature& in,
                         Feature* out) {
  if (out == nullptr) return false;
  CopyFeatureInto(layer, in, out);
  return true;
}

bool TranslateMvtToFeatureTile(const MvtTile& in, FeatureTile* out) {
  if (out == nullptr) return false;
  out->features.clear();
  size_t total = 0;
  for (const MvtLayer& layer : in.layers) total += layer.features.size();
  out->features.reserve(total);
  for (const MvtLayer& layer : in.layers) {
    for (const MvtFeature& feature : layer.features) {
      out->features.push_back(CopyFeature(layer, feature));
    }
  }
  return true;
}

}  // namespace orcmap
