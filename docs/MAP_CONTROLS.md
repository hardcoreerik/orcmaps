# Map controls

OrcMaps camera controls operate on the existing `orcmap::Viewport`. They do
not depend on touch, buttons, a graphics library, or a board. Applications map
their input devices onto the same portable functions.

Implemented controls:

- `SetCenter` / `GetCenter`
- `SetZoom` / `GetZoom` / `ZoomIn` / `ZoomOut`
- `SetViewportSize`
- `PanByPixels`
- `GetVisibleBounds`
- `FitBounds` / `FillBounds` / `MinFillZoom`
- `ProjectLatLon` / `ScreenToLatLon`
- `ZoomAtScreenPoint`

Longitude wraps at the antimeridian and latitude clamps to the Web Mercator
limit. Zoom is integer-only from z0 through z31. Positive `PanByPixels(dx, dy)`
moves the camera east and south; a direct-manipulation touch binding normally
passes the negative drag delta.

Visible bounds use `min_lon > max_lon` to represent an antimeridian-crossing
view. A viewport covering the whole world reports `-180..180`.

`FitBounds` and `FillBounds` are complements, and the distinction matters to
applications:

- `FitBounds` centres on the bounds and chooses the **highest** integer zoom
  at which all of them fit after padding. Use it to show a pack's whole
  coverage area. The result normally leaves empty margin, because a pack's
  aspect ratio rarely matches a screen's.
- `FillBounds` centres on the bounds and chooses the **smallest** zoom at
  which the bounds fully cover the viewport, so the screen is filled with map
  data and no margin shows. Use it for a normal map view. It returns false
  when no zoom can cover the viewport, and callers then fall back to
  `FitBounds`.

Both use projected latitude, not an inaccurate arithmetic latitude midpoint,
and both derive zoom from the caller's own `width_px`/`height_px`: an
application supplies its screen size and gets a correctly centred, correctly
sized map without a per-board centre or zoom constant. The same pack framed
on a 1280x600 display and a 320x170 display yields the same centre and
different zooms.

`MinFillZoom` is the query form of `FillBounds`: it reports the zoom
`FillBounds` would choose **without moving the camera**. Applications use it to
derive a zoom-out floor for a UI control, because below that zoom the screen
must show empty area no matter how the user pans — the installed bounds simply
cannot cover the display. It ignores pack `min_zoom`/`max_zoom`; clamping to
what is actually stored belongs to the caller, which knows the catalogue.

A pack spanning the full 360 degrees of longitude is exempt from the width
test, because it wraps around itself and so covers any width; its floor is
height-driven and equals `WorldViewZoom` (z2 on 1280x600). A pack stopping
even slightly short of the antimeridian does not get that exemption. A small
extract yields a much higher floor on the same screen — the 0.095 deg x
0.06 deg Springfield extract first covers 1280x600 at z15, and covers 86.5% of
it at z14, 35.0% at z13 and 8.8% at z12. A UI that lets zoom-out run past the
floor is not failing to redraw; it is correctly showing that no installed pack
reaches that far out. Provisioning, not rendering, is what lowers the floor.

## World view and wrapping around

`WorldViewZoom` gives the natural whole-world view for a display: the smallest
zoom whose world is at least as **tall** as the viewport. Width is deliberately
not tested, because the map repeats horizontally — Mercator X wraps, Y is
clamped and cannot. On a 1280x600 map area this is z2, where the 1024 px world
is entirely visible and the remaining 256 px is filled by the wrap instead of
being left empty. On a 320x170 display it is z0.

Wrapping is why there are two enumeration functions, and choosing the wrong
one is the easiest mistake here:

| function | returns | use for |
|---|---|---|
| `EnumerateVisibleTiles` | unique `TileId`s, one position each | deciding what to **fetch and decode** |
| `EnumerateVisibleTilePlacements` | one `TilePlacement` per drawn instance, repeats included | deciding what to **draw, and where** |

A `TilePlacement` pairs a wrapped `tile` (what you look up in a pack) with an
`unwrapped_x` (which world copy it is). Draw it with `RenderFeatureTileAt` or
place it with `MakeTilePlacementScreenMap`; the plain `RenderFeatureTile` and
`MakeTileScreenMap` place a tile at the copy nearest the centre, which is
correct only when nothing repeats. `NearestTilePlacement` exposes that
single-copy rule.

Decode each distinct tile **once** and draw it at each of its placements.
Fetching per placement would multiply the expensive stages for no benefit;
the benchmark harness records `tiles_visible` (distinct, so it stays
comparable with runs predating placements) alongside `placements` (drawn
instances) so the difference is visible in the data.

`ZoomAtScreenPoint` preserves the geographic point below the supplied screen
coordinate. This is the portable behavior needed by touch, mouse-wheel, and
button-driven map applications; no UI widgets live in OrcMaps core.

Host tests cover z0 world bounds, repeated zoom limits, antimeridian pan,
Mercator clamping, world/Oregon/Springfield fit, `FillBounds` centring and
genuine coverage, `FillBounds` screen-dependence across two display sizes,
unfillable bounds, tiny displays, projection round trips, anchored zoom,
`MinFillZoom` agreeing with `FillBounds` while leaving the camera untouched,
floors that rise as coverage shrinks or the screen grows, `WorldViewZoom`
being height-driven and width-independent, world-spanning bounds filling by
wrapping, placements covering a viewport wider than the world with at least
one tile repeated, and placements agreeing exactly with the nearest-copy
mapping when nothing repeats.
Touch drag/zoom bindings are physically exercised by the Tab5 demo; no
automated physical touch-control test exists.
