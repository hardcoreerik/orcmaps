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

No global overview cutoff is selected yet. Candidate ranges such as z0-4,
z0-6, z0-8, and z0-10 must be generated and compared for size, tile count,
visible usefulness, host render time, and embedded cost before one is chosen.
The first source bundle is reproducibly pinned to 21 official Natural Earth
5.1.2 archives: land, ocean, lakes, rivers/lake centerlines, admin-0 lines,
admin-1 lines, and populated places at 110m, 50m, and 10m. It is acquired to
the gitignored `data/local/world-overview/natural-earth/5.1.2/` path using
`tools/pack-builder/acquire_world_overview_sources.py`. No overview PMTiles or
full planet archive has been generated, and no global-render claim is made.

A future Pack Service may distribute or generate immutable pack triplets. It
is optional provisioning only:

```
optional internet -> provisioning/update -> local immutable packs -> OrcMaps
```

An installed pack never depends on a remote catalog, authentication, license
server, attribution lookup, telemetry endpoint, update check, or remote tile
server.
