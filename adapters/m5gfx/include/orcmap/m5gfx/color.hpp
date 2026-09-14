#pragma once

#include <cstdint>

#include "orcmap/color.hpp"

namespace orcmap::m5gfx_adapter {

// Converts an orcmap::Color (plain RGBA8) to M5GFX/LovyanGFX RGB565.
// Lives in the adapter, not the core. Alpha is ignored: this adapter
// renders opaque RGB565 only. Color.a is part of the generic Color type
// for future overlay compositing; it is not blended here.

constexpr uint16_t ToRgb565(Color c) {
  return static_cast<uint16_t>(((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) | (c.b >> 3));
}

}  // namespace orcmap::m5gfx_adapter
