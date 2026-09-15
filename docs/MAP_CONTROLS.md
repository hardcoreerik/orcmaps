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

The floor is a real limit and not an off-by-one: one z0 tile is 256 px, so even
a whole-world pack cannot cover a 1280 px-wide map area until z3. A small
extract yields a much higher floor on the same screen — the 0.095 deg x
0.06 deg Springfield extract first covers 1280x600 at z15, and covers 86.5% of
it at z14, 35.0% at z13 and 8.8% at z12. A UI that lets zoom-out run past the
floor is not failing to redraw; it is correctly showing that no installed pack
reaches that far out. Provisioning, not rendering, is what lowers the floor.

`ZoomAtScreenPoint` preserves the geographic point below the supplied screen
coordinate. This is the portable behavior needed by touch, mouse-wheel, and
button-driven map applications; no UI widgets live in OrcMaps core.

Host tests cover z0 world bounds, repeated zoom limits, antimeridian pan,
Mercator clamping, world/Oregon/Springfield fit, `FillBounds` centring and
genuine coverage, `FillBounds` screen-dependence across two display sizes,
unfillable bounds, tiny displays, projection round trips, anchored zoom,
`MinFillZoom` agreeing with `FillBounds` while leaving the camera untouched,
and floors that rise as coverage shrinks or the screen grows.
Touch drag/zoom bindings are physically exercised by the Tab5 demo; no
automated physical touch-control test exists.
