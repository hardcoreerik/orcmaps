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
- `FitBounds`
- `ProjectLatLon` / `ScreenToLatLon`
- `ZoomAtScreenPoint`

Longitude wraps at the antimeridian and latitude clamps to the Web Mercator
limit. Zoom is integer-only from z0 through z31. Positive `PanByPixels(dx, dy)`
moves the camera east and south; a direct-manipulation touch binding normally
passes the negative drag delta.

Visible bounds use `min_lon > max_lon` to represent an antimeridian-crossing
view. A viewport covering the whole world reports `-180..180`. `FitBounds`
chooses the highest integer zoom that fits after padding and uses projected
latitude, not an inaccurate arithmetic latitude midpoint.

`ZoomAtScreenPoint` preserves the geographic point below the supplied screen
coordinate. This is the portable behavior needed by touch, mouse-wheel, and
button-driven map applications; no UI widgets live in OrcMaps core.

Host tests cover z0 world bounds, repeated zoom limits, antimeridian pan,
Mercator clamping, world/Oregon/Springfield fit, tiny displays, projection
round trips, and anchored zoom. No physical touch-control test has run yet.
