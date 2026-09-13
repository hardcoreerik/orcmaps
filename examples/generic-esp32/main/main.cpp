// Minimal ESP-IDF smoke test for the `orcmap` component. Proves a generic
// ESP-IDF application can compile and link the portable core (geo math,
// style system) with no graphics framework -- see STATUS.md and
// docs/ARCHITECTURE.md "Build system".
//
// Deliberately does NOT: open a map pack (no adapters/esp_idf ByteSource
// yet), decode MVT at runtime, or draw pixels. Those are later examples
// (graphics-specific work belongs in examples/m5stack-tab5, not here).

#include "esp_log.h"
#include "orcmap/geo.hpp"
#include "orcmap/style.hpp"

static const char* kTag = "orcmap_example";

extern "C" void app_main(void) {
  // Geo/tile math -- see include/orcmap/geo.hpp.
  const orcmap::TileId tile = orcmap::LatLonToTile(44.0521, -123.0868, 10);
  ESP_LOGI(kTag, "Eugene, OR at zoom 10 -> tile z=%u x=%u y=%u",
           tile.z, static_cast<unsigned>(tile.x), static_cast<unsigned>(tile.y));

  // Style system -- see include/orcmap/style.hpp.
  const orcmap::MapStyle& style = orcmap::styles::OrcSdrDark();
  const orcmap::MapPaint paint =
      orcmap::ResolveFeatureStyle(orcmap::FeatureKind::kMotorway, 10, style);
  ESP_LOGI(kTag, "OrcSDR Dark motorway paint: visible=%d color=#%02x%02x%02x",
           paint.visible, paint.color.r, paint.color.g, paint.color.b);

  ESP_LOGI(kTag, "orcmap component built and ran successfully on this ESP-IDF target");
}
