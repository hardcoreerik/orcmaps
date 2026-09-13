#include "orcmap/style.hpp"

#include <cstring>

#include "orcmap/cache_key.hpp"
#include "test_util.hpp"

namespace {

void TestBuiltinStyleIdsResolve() {
  ORCMAP_EXPECT_TRUE(std::strcmp(orcmap::styles::OrcSdrDark().id, "orcsdr-dark") == 0);
  ORCMAP_EXPECT_TRUE(std::strcmp(orcmap::styles::StandardLight().id, "standard-light") == 0);
  ORCMAP_EXPECT_TRUE(std::strcmp(orcmap::styles::HighContrastField().id, "high-contrast-field") == 0);
  ORCMAP_EXPECT_TRUE(std::strcmp(orcmap::styles::NightRedSafe().id, "night-red-safe") == 0);

  ORCMAP_EXPECT_TRUE(
      std::strcmp(orcmap::GetBuiltinStyle(orcmap::BuiltinStyle::kOrcSdrDark).id,
                  "orcsdr-dark") == 0);
  ORCMAP_EXPECT_TRUE(orcmap::FindBuiltinStyleById("night-red-safe") != nullptr);
  ORCMAP_EXPECT_TRUE(orcmap::FindBuiltinStyleById("does-not-exist") == nullptr);
}

void TestDefaultStyleExists() {
  const orcmap::MapStyle& def = orcmap::DefaultStyle();
  ORCMAP_EXPECT_TRUE(std::strcmp(def.id, "standard-light") == 0);
}

void TestStyleManagerSwitching() {
  orcmap::StyleManager manager;
  ORCMAP_EXPECT_TRUE(std::strcmp(manager.Active().id, "standard-light") == 0);

  manager.SetStyle(orcmap::BuiltinStyle::kOrcSdrDark);
  ORCMAP_EXPECT_TRUE(std::strcmp(manager.Active().id, "orcsdr-dark") == 0);

  // Invalid id must not change the active style.
  const bool ok = manager.SetStyleById("not-a-real-style");
  ORCMAP_EXPECT_TRUE(!ok);
  ORCMAP_EXPECT_TRUE(std::strcmp(manager.Active().id, "orcsdr-dark") == 0);

  const bool ok2 = manager.SetStyleById("night-red-safe");
  ORCMAP_EXPECT_TRUE(ok2);
  ORCMAP_EXPECT_TRUE(std::strcmp(manager.Active().id, "night-red-safe") == 0);
}

void TestZoomVisibility() {
  using orcmap::FeatureKind;
  const orcmap::MapStyle& dark = orcmap::styles::OrcSdrDark();

  // Minor roads hidden at low zoom, visible once zoomed in.
  const orcmap::MapPaint low_zoom_minor =
      orcmap::ResolveFeatureStyle(FeatureKind::kMinorRoad, 2, dark);
  ORCMAP_EXPECT_TRUE(!low_zoom_minor.visible);
  const orcmap::MapPaint high_zoom_minor =
      orcmap::ResolveFeatureStyle(FeatureKind::kMinorRoad, 14, dark);
  ORCMAP_EXPECT_TRUE(high_zoom_minor.visible);

  // Major roads remain visible at low zoom.
  const orcmap::MapPaint low_zoom_motorway =
      orcmap::ResolveFeatureStyle(FeatureKind::kMotorway, 2, dark);
  ORCMAP_EXPECT_TRUE(low_zoom_motorway.visible);

  // Buildings only appear at high zoom.
  const orcmap::MapPaint low_zoom_building =
      orcmap::ResolveFeatureStyle(FeatureKind::kBuilding, 10, dark);
  ORCMAP_EXPECT_TRUE(!low_zoom_building.visible);

  // High-Contrast/Field disables rail entirely, and pushes minor roads to
  // an even higher zoom than the standard styles.
  const orcmap::MapStyle& field = orcmap::styles::HighContrastField();
  ORCMAP_EXPECT_TRUE(!orcmap::ResolveFeatureStyle(FeatureKind::kRail, 20, field).visible);
  ORCMAP_EXPECT_TRUE(!orcmap::ResolveFeatureStyle(FeatureKind::kMinorRoad, 12, field).visible);
  ORCMAP_EXPECT_TRUE(orcmap::ResolveFeatureStyle(FeatureKind::kMinorRoad, 14, field).visible);
}

void TestNightStyleHasNoBlueLight() {
  // Every color in Night/Red-Safe must be red-dominant with zero-or-low
  // blue, per its design brief ("no blue light", "no bright white").
  const orcmap::MapStyle& night = orcmap::styles::NightRedSafe();
  for (const auto& rule : night.rules) {
    ORCMAP_EXPECT_TRUE(rule.color.b <= rule.color.r);
    ORCMAP_EXPECT_TRUE(!(rule.color.r > 240 && rule.color.g > 240 && rule.color.b > 240));
  }
}

void TestStylesAreDistinct() {
  // The four built-ins must not accidentally collapse into duplicates
  // (e.g. a copy-paste that never changed the background color).
  const orcmap::Color dark_bg = orcmap::styles::OrcSdrDark().rules[0].color;
  const orcmap::Color light_bg = orcmap::styles::StandardLight().rules[0].color;
  const orcmap::Color field_bg = orcmap::styles::HighContrastField().rules[0].color;
  const orcmap::Color night_bg = orcmap::styles::NightRedSafe().rules[0].color;
  ORCMAP_EXPECT_TRUE(dark_bg != light_bg);
  ORCMAP_EXPECT_TRUE(dark_bg != field_bg);
  ORCMAP_EXPECT_TRUE(dark_bg != night_bg);
  ORCMAP_EXPECT_TRUE(light_bg != field_bg);
  ORCMAP_EXPECT_TRUE(light_bg != night_bg);
  ORCMAP_EXPECT_TRUE(field_bg != night_bg);
}

void TestCacheKeyChangesWithStyle() {
  const orcmap::MapStyle& dark = orcmap::styles::OrcSdrDark();
  const orcmap::MapStyle& light = orcmap::styles::StandardLight();

  const auto key_dark =
      orcmap::RenderedTileCacheKey::Make("lane_county", 10, 163, 355, dark);
  const auto key_light =
      orcmap::RenderedTileCacheKey::Make("lane_county", 10, 163, 355, light);
  ORCMAP_EXPECT_TRUE(key_dark != key_light);

  // Same style, same tile -> identical key (cache hit expected).
  const auto key_dark_again =
      orcmap::RenderedTileCacheKey::Make("lane_county", 10, 163, 355, dark);
  ORCMAP_EXPECT_TRUE(key_dark == key_dark_again);

  // Bumping a style's version (simulating an edited built-in style)
  // invalidates the cache key even though the id is unchanged.
  orcmap::MapStyle dark_v2 = dark;
  dark_v2.version = dark.version + 1;
  const auto key_dark_v2 =
      orcmap::RenderedTileCacheKey::Make("lane_county", 10, 163, 355, dark_v2);
  ORCMAP_EXPECT_TRUE(key_dark != key_dark_v2);

  // Different tile coordinates -> different key.
  const auto key_other_tile =
      orcmap::RenderedTileCacheKey::Make("lane_county", 10, 163, 356, dark);
  ORCMAP_EXPECT_TRUE(key_dark != key_other_tile);

  // Different pack -> different key, even for the same z/x/y/style.
  const auto key_other_pack =
      orcmap::RenderedTileCacheKey::Make("oregon", 10, 163, 355, dark);
  ORCMAP_EXPECT_TRUE(key_dark != key_other_pack);
}

}  // namespace

void RunStyleTests() {
  TestBuiltinStyleIdsResolve();
  TestDefaultStyleExists();
  TestStyleManagerSwitching();
  TestZoomVisibility();
  TestNightStyleHasNoBlueLight();
  TestStylesAreDistinct();
  TestCacheKeyChangesWithStyle();
}
