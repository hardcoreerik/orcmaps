// Minimal ESP-IDF smoke test for the `orcmap` component. Proves the
// portable core (geo math, PMTiles container reader, MVT decoder, style
// system) actually compiles and links against a real ESP-IDF toolchain --
// see docs/STATUS.md "Current blockers" for why this didn't exist before
// and docs/ARCHITECTURE.md "Build system" for what it closes.
//
// Deliberately does NOT exercise file I/O (no adapters/esp_idf ByteSource
// exists yet -- that's the next real gap, not this example's job) or
// The generic smoke test does not yet exercise rendering; the initial
// adapters/m5gfx backend now exists, but this example currently validates
// the portable core and ESP-IDF integration only.
// those adapters are built, per README.md's stated intent for
// examples/generic-esp32.

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
