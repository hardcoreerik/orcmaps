#pragma once

#include <cstdint>

#include "orcmap/color.hpp"

namespace orcmap::m5gfx_adapter {

// Converts an orcmap::Color (plain RGBA8, include/orcmap/color.hpp) to
// M5GFX/LovyanGFX's native RGB565 pixel format. This conversion lives
// here, in the adapter, not in the core -- see docs/PROJECT_TRUTH.md
// "Rendering Model": orcmap::Color has zero display-API dependency, and
// must stay that way so a future LVGL or host-side test renderer needs no
// change to the style system.
//
// Alpha is ignored: LovyanGFX's basic draw calls (drawLine, fillRect,
// etc.) take an opaque RGB565 color; alpha blending is a separate
// LovyanGFX feature this minimal adapter does not use.
constexpr uint16_t ToRgb565(Color c) {
  return static_cast<uint16_t>(((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) | (c.b >> 3));
}

}  // namespace orcmap::m5gfx_adapter
