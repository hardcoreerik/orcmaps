# Global map strategy

OrcMaps ships worldwide capability as independent local packs:

```
global overview + optional regional detail + optional full-detail planet
```

The normal product must not require an 80 GB-class planet download. A compact
overview provides useful low-zoom land, water, boundaries, major transport,
and major places. Regional packs provide higher zooms where the user needs
them. A full planet archive remains an optional stress-test/product edition.

Packs describe geography and content, never a graphics adapter or board.
M5GFX, LVGL, a native RGB565 framebuffer, and future targets consume the same
PMTiles/MVT artifacts through OrcMaps core.

Measured z0-z6, z0-z7, and z0-z8 candidates are 4.61, 9.29, and 16.37 MiB.
Current evidence recommends z6 for a tiny pack, z7 for a standard pack, and a
regional handoff at z8. This is not frozen release policy; embedded measurements
can still change it.
The first source bundle is reproducibly pinned to 21 official Natural Earth
5.1.2 archives: land, ocean, lakes, rivers/lake centerlines, admin-0 lines,
admin-1 lines, and populated places at 110m, 50m, and 10m. It is acquired to
the gitignored `data/local/world-overview/natural-earth/5.1.2/` path using
`tools/pack-builder/acquire_world_overview_sources.py`, then built by
`build_world_overview.py` into graphics-independent `orcmaps-overview-1` MVT.
The generic host renderer verifies global rendering. No full-detail planet
archive has been generated.

A future Pack Service may distribute or generate immutable pack triplets. It
is optional provisioning only:

```
optional internet -> provisioning/update -> local immutable packs -> OrcMaps
```

An installed pack never depends on a remote catalog, authentication, license
server, attribution lookup, telemetry endpoint, update check, or remote tile
server.
