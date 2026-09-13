#pragma once

#include <string>

namespace orcmap {

// A single attribution requirement, surfaced by the engine so a consuming
// application never has to hard-code a credit string like
// "(c) OpenStreetMap contributors" -- see docs/DATA_AND_LICENSING.md
// "Runtime attribution". The engine determines WHETHER attribution is
// required and WHAT it says (derived from a pack's manifest at load time,
// which in turn derives it from the source's data-provenance record); the
// application/renderer adapter decides HOW to display it. This keeps
// attribution rendering out of the core engine, preserving UI flexibility
// (docs/STYLING.md draws the same line for map styling).
struct AttributionInfo {
  bool required = false;
  std::string text;  // e.g. "(c) OpenStreetMap contributors"
  std::string url;   // Optional reference URL, e.g. the source's copyright page.
};

}  // namespace orcmap
