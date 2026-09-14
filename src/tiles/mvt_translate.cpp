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

Feature CopyFeature(const MvtLayer& layer, const MvtFeature& in) {
  Feature out;
  out.id = in.id;
  out.geom_type = MapGeomType(in.geom_type);
  out.kind_assigned = false;
  out.extent = layer.extent == 0 ? 4096 : layer.extent;
  out.layer = layer.name;
  out.geometry.type = out.geom_type;
  out.geometry.paths.reserve(in.geometry.size());
  for (const MvtRing& ring : in.geometry) {
    Path path;
    path.reserve(ring.size());
    for (const MvtPoint& p : ring) {
      path.push_back(Point{p.x, p.y});
    }
    out.geometry.paths.push_back(std::move(path));
  }
  out.property_keys = in.attribute_keys;
  out.property_values.reserve(in.attribute_values.size());
  for (const MvtValue& value : in.attribute_values) {
    out.property_values.push_back(value);
  }
  return out;
}

}  // namespace

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
