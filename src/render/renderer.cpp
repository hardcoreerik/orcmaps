#include "orcmap/renderer.hpp"

#include <vector>

namespace orcmap {

namespace {

void ProjectPath(const Viewport& viewport, TileId tile, uint32_t extent,
                 const Path& path, std::vector<int>* xy) {
  xy->clear();
  xy->reserve(path.size() * 2);
  for (const Point& p : path) {
    int sx = 0;
    int sy = 0;
    TileLocalToScreen(viewport, tile, extent, p.x, p.y, &sx, &sy);
    xy->push_back(sx);
    xy->push_back(sy);
  }
}

void DrawLines(RenderTarget* target, const std::vector<int>& xy, Color color,
               bool closed) {
  const size_t n = xy.size() / 2;
  if (n < 2) return;
  for (size_t i = 0; i + 1 < n; ++i) {
    target->DrawLine(xy[i * 2], xy[i * 2 + 1], xy[(i + 1) * 2],
                     xy[(i + 1) * 2 + 1], color);
  }
  if (closed && n > 2) {
    target->DrawLine(xy[(n - 1) * 2], xy[(n - 1) * 2 + 1], xy[0], xy[1],
                     color);
  }
}

void RenderOneFeature(const Feature& feature, TileId tile,
                      const Viewport& viewport, const MapStyle& style,
                      RenderTarget* target, std::vector<int>* scratch) {
  if (!feature.kind_assigned) return;
  const MapPaint paint =
      ResolveFeatureStyle(feature.kind, viewport.zoom, style);
  if (!paint.visible) return;

  switch (feature.geometry.type) {
    case GeomType::kPoint:
      for (const Path& path : feature.geometry.paths) {
        ProjectPath(viewport, tile, feature.extent, path, scratch);
        const size_t n = scratch->size() / 2;
        for (size_t i = 0; i < n; ++i) {
          target->DrawPoint((*scratch)[i * 2], (*scratch)[i * 2 + 1],
                            paint.color);
        }
      }
      break;
    case GeomType::kLineString:
      for (const Path& path : feature.geometry.paths) {
        ProjectPath(viewport, tile, feature.extent, path, scratch);
        DrawLines(target, *scratch, paint.color, false);
      }
      break;
    case GeomType::kPolygon:
      // First path only: simple outer ring. Extra paths (holes) are not
      // subtracted and are not filled -- locked by tests/host/test_render.cpp.
      if (feature.geometry.paths.empty()) break;
      ProjectPath(viewport, tile, feature.extent, feature.geometry.paths[0],
                  scratch);
      target->FillPolygon(scratch->data(), scratch->size() / 2, paint.color);
      break;
    case GeomType::kUnknown:
    default:
      break;
  }
}

}  // namespace

void RenderFeatureTile(const FeatureTile& features, TileId source_tile,
                       const Viewport& viewport, const MapStyle& style,
                       RenderTarget* target) {
  if (target == nullptr) return;
  const MapPaint background =
      ResolveFeatureStyle(FeatureKind::kBackground, viewport.zoom, style);
  if (background.visible) {
    target->FillRect(0, 0, target->Width(), target->Height(), background.color);
  }
  std::vector<int> scratch;
  for (const Feature& feature : features.features) {
    RenderOneFeature(feature, source_tile, viewport, style, target, &scratch);
  }
}

}  // namespace orcmap
