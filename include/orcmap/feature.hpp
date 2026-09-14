#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "orcmap/feature_kind.hpp"

namespace orcmap {

// OrcMaps-owned map feature / geometry model. Format-independent: nothing
// in this header is MVT, PMTiles, M5GFX, ESP-IDF, or application-domain
// (aircraft, LoRa, RF). See docs/ARCHITECTURE.md "Feature / geometry model".
//
// Geometry answers "what shape is this?" FeatureKind answers "what does
// this represent for styling?" Those are separate. A polygon is not
// automatically water, a building, or a park. This header depends on
// feature_kind.hpp, not on the style subsystem.
//
// Coordinates are tile-local integers in [0, extent) nominally (buffer
// overflow past the tile edge is allowed, matching common vector-tile
// practice). Conversion to screen pixels or lat/lon is a Viewport /
// renderer concern, not this model's.
//
// Storage layout (flattened FeatureTile of Features, each carrying its
// own layer string and extent) is provisional. The types are architectural;
// packing/interning can change after real-tile measurement. Do not treat
// the current layout as a frozen ABI.

enum class GeomType : uint8_t {
  kUnknown = 0,
  kPoint = 1,
  kLineString = 2,
  kPolygon = 3,
};

struct Point {
  int32_t x = 0;
  int32_t y = 0;
};

// One path: a single point list, a linestring, or one polygon ring.
// Polygon outer/inner ring classification is not computed here.
using Path = std::vector<Point>;

struct Geometry {
  GeomType type = GeomType::kUnknown;
  std::vector<Path> paths;
};

// Property values after format decode. Parallel key/value arrays rather
// than a map: small linear scans are cheaper on embedded hardware for the
// handful of attributes a real feature carries.
using PropertyValue = std::variant<std::monostate, std::string, double, int64_t,
                                    uint64_t, bool>;

struct Feature {
  uint64_t id = 0;
  // Semantic class for the style system. Unassigned until a classifier
  // (experimental today; schema decision still deferred) sets it.
  FeatureKind kind = FeatureKind::kBackground;
  bool kind_assigned = false;
  uint32_t extent = 4096;
  std::string layer;  // Source layer name; not a format-specific type.
  Geometry geometry;
  std::vector<std::string> property_keys;
  std::vector<PropertyValue> property_values;
};

struct FeatureTile {
  std::vector<Feature> features;
};

inline const PropertyValue* FindProperty(const Feature& feature, const char* key) {
  if (key == nullptr) return nullptr;
  const size_t n = feature.property_keys.size() < feature.property_values.size()
                       ? feature.property_keys.size()
                       : feature.property_values.size();
  for (size_t i = 0; i < n; ++i) {
    if (feature.property_keys[i] == key) return &feature.property_values[i];
  }
  return nullptr;
}

}  // namespace orcmap
