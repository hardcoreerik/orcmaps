#include "orcmap/renderer.hpp"

#include <vector>

namespace orcmap {

namespace {

void DrawPointMapped(RenderTarget* target, const TileScreenMap& map,
                     int32_t local_x, int32_t local_y, Color color) {
  int sx = 0;
  int sy = 0;
  ProjectLocal(map, local_x, local_y, &sx, &sy);
  target->DrawPoint(sx, sy, color);
}

void DrawLineMapped(RenderTarget* target, const TileScreenMap& map, int32_t x0,
                    int32_t y0, int32_t x1, int32_t y1, Color color,
                    float width_px) {
  int sx0 = 0;
  int sy0 = 0;
  int sx1 = 0;
  int sy1 = 0;
  ProjectLocal(map, x0, y0, &sx0, &sy0);
  ProjectLocal(map, x1, y1, &sx1, &sy1);
  target->DrawLine(sx0, sy0, sx1, sy1, color, width_px);
}

void RenderOneFeature(const Feature& feature, const TileScreenMap& map,
                      const Viewport& viewport, const MapStyle& style,
                      RenderTarget* target, std::vector<int>* poly_xy) {
  if (!feature.kind_assigned) return;
  if (feature.kind == FeatureKind::kLabelPrimary ||
      feature.kind == FeatureKind::kLabelSecondary ||
      feature.kind == FeatureKind::kLabelMuted) {
    return;
  }
  const MapPaint paint =
      ResolveFeatureStyle(feature.kind, viewport.zoom, style);
  if (!paint.visible) return;

  switch (feature.geometry.type) {
    case GeomType::kPoint:
      for (const Path& path : feature.geometry.paths) {
        for (const Point& p : path) {
          DrawPointMapped(target, map, p.x, p.y, paint.color);
        }
      }
      break;
    case GeomType::kLineString:
      for (const Path& path : feature.geometry.paths) {
        if (path.size() < 2) continue;
        for (size_t i = 0; i + 1 < path.size(); ++i) {
          DrawLineMapped(target, map, path[i].x, path[i].y, path[i + 1].x,
                         path[i + 1].y, paint.color, paint.width_px);
        }
      }
      break;
    case GeomType::kPolygon:
      // First path only: simple outer ring. Extra paths (holes) are not
      // subtracted and are not filled -- locked by tests/host/test_render.cpp.
      if (feature.geometry.paths.empty()) break;
      {
        const Path& ring = feature.geometry.paths[0];
        if (ring.size() < 3) break;
        poly_xy->clear();
        poly_xy->reserve(ring.size() * 2);
        for (const Point& p : ring) {
          int sx = 0;
          int sy = 0;
          ProjectLocal(map, p.x, p.y, &sx, &sy);
          poly_xy->push_back(sx);
          poly_xy->push_back(sy);
        }
        target->FillPolygon(poly_xy->data(), poly_xy->size() / 2, paint.color);
      }
      break;
    case GeomType::kUnknown:
    default:
      break;
  }
}

}  // namespace

bool ClearMapBackground(const Viewport& viewport, const MapStyle& style,
                        RenderTarget* target) {
  if (target == nullptr) return false;
  if (!ZoomIsValid(viewport.zoom)) return false;
  const MapPaint background =
      ResolveFeatureStyle(FeatureKind::kBackground, viewport.zoom, style);
  if (background.visible) {
    target->FillRect(0, 0, target->Width(), target->Height(), background.color);
  }
  return true;
}

bool RenderFeatureTile(const FeatureTile& features, TileId source_tile,
                       const Viewport& viewport, const MapStyle& style,
                       RenderTarget* target) {
  if (target == nullptr) return false;
  if (!ZoomIsValid(viewport.zoom)) return false;
  if (source_tile.z != viewport.zoom) return false;

  TileScreenMap map;
  uint32_t prepared_extent = 0;
  bool have_map = false;
  std::vector<int> poly_xy;
  for (const Feature& feature : features.features) {
    const uint32_t extent = feature.extent == 0 ? 4096u : feature.extent;
    if (!have_map || extent != prepared_extent) {
      if (!MakeTileScreenMap(viewport, source_tile, extent, &map)) {
        return false;
      }
      prepared_extent = extent;
      have_map = true;
    }
    RenderOneFeature(feature, map, viewport, style, target, &poly_xy);
  }
  return true;
}

}  // namespace orcmap
