#include "orcmap/experimental/mvt_classify.hpp"

#include <cstring>

namespace orcmap {
namespace experimental {

namespace {

bool LayerIs(const Feature& feature, const char* name) {
  return feature.layer == name;
}

const char* StringProp(const Feature& feature, const char* key) {
  const PropertyValue* value = FindProperty(feature, key);
  if (value == nullptr || !std::holds_alternative<std::string>(*value)) {
    return nullptr;
  }
  return std::get<std::string>(*value).c_str();
}

FeatureKind ClassifyRoad(const Feature& feature) {
  const char* cls = StringProp(feature, "class");
  if (cls == nullptr) return FeatureKind::kMinorRoad;
  if (std::strcmp(cls, "motorway") == 0 || std::strcmp(cls, "trunk") == 0) {
    return FeatureKind::kMotorway;
  }
  if (std::strcmp(cls, "primary") == 0) return FeatureKind::kPrimaryRoad;
  if (std::strcmp(cls, "secondary") == 0 || std::strcmp(cls, "tertiary") == 0) {
    return FeatureKind::kSecondaryRoad;
  }
  return FeatureKind::kMinorRoad;
}

}  // namespace

bool TryClassifyFeature(const Feature& feature, FeatureKind* kind) {
  if (kind == nullptr) return false;
  if (LayerIs(feature, "water") || LayerIs(feature, "waterway") ||
      LayerIs(feature, "ocean")) {
    *kind = FeatureKind::kWater;
    return true;
  }
  if (LayerIs(feature, "road") || LayerIs(feature, "roads") ||
      LayerIs(feature, "transportation") || LayerIs(feature, "transport")) {
    *kind = ClassifyRoad(feature);
    return true;
  }
  if (LayerIs(feature, "rail") || LayerIs(feature, "railway")) {
    *kind = FeatureKind::kRail;
    return true;
  }
  if (LayerIs(feature, "building") || LayerIs(feature, "buildings")) {
    *kind = FeatureKind::kBuilding;
    return true;
  }
  if (LayerIs(feature, "park")) {
    *kind = FeatureKind::kPark;
    return true;
  }
  if (LayerIs(feature, "land") || LayerIs(feature, "earth") ||
      LayerIs(feature, "landcover")) {
    *kind = FeatureKind::kLand;
    return true;
  }
  if (LayerIs(feature, "boundary") || LayerIs(feature, "boundaries")) {
    *kind = FeatureKind::kBoundary;
    return true;
  }
  if (LayerIs(feature, "airport") || LayerIs(feature, "aerodrome") ||
      LayerIs(feature, "aerodrome_label")) {
    *kind = FeatureKind::kAirport;
    return true;
  }
  if (LayerIs(feature, "place") || LayerIs(feature, "place_label") ||
      LayerIs(feature, "poi")) {
    *kind = FeatureKind::kLabelPrimary;
    return true;
  }
  return false;
}

bool AssignFeatureKinds(FeatureTile* tile) {
  if (tile == nullptr) return false;
  for (Feature& feature : tile->features) {
    FeatureKind kind = FeatureKind::kBackground;
    if (TryClassifyFeature(feature, &kind)) {
      feature.kind = kind;
      feature.kind_assigned = true;
    }
  }
  return true;
}

}  // namespace experimental
}  // namespace orcmap
