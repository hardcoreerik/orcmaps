#pragma once

#include <cstdint>

namespace orcmap {

// Renderer-agnostic color. Deliberately not RGB565 (M5GFX/LovyanGFX's
// native pixel format) -- the core style system must not assume any
// particular display's pixel format. A display adapter (e.g.
// orcmap/m5gfx) converts to its own native format at draw time. Alpha is
// stored but the current M5GFX adapter renders opaque RGB565 only. See
// docs/STYLING.md "Renderer relationship".
struct Color {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t a = 255;

  static constexpr Color Rgb(uint8_t r, uint8_t g, uint8_t b) {
    return Color{r, g, b, 255};
  }
  static constexpr Color Rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return Color{r, g, b, a};
  }

  constexpr bool operator==(const Color& other) const {
    return r == other.r && g == other.g && b == other.b && a == other.a;
  }
  constexpr bool operator!=(const Color& other) const {
    return !(*this == other);
  }
};

}  // namespace orcmap
