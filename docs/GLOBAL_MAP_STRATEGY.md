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

No global overview cutoff is selected yet. The first source bundle is
reproducibly pinned to 21 official Natural Earth 5.1.2 archives: land, ocean,
lakes, rivers/lake centerlines, admin-0 lines, admin-1 lines, and populated
places at 110m, 50m, and 10m. It is acquired to the gitignored
`data/local/world-overview/natural-earth/5.1.2/` path using
`tools/pack-builder/acquire_world_overview_sources.py`.

Five overview candidates **have** now been generated host-side as immutable
archive/manifest/checksum triplets under
`data/local/world-overview/build/` (gitignored), all
`schema_version: orcmaps-overview-1`, `pack_class: clean`, sourced solely
from the `natural-earth` provenance record:

| Candidate | Size |
|---|---:|
| z0-4 | 871,343 B |
| z0-5 | 1,492,862 B |
| z0-6 | 4,833,728 B |
| z0-7 | 9,737,500 B |
| z0-8 | 17,165,758 B |

Comparison is therefore **partial**: sizes are measured and host renders
exist at z4/z5/z6 (world, North America, Pacific Northwest, Oregon), but
tile counts and host render times were never recorded, no z0-10 candidate
was generated, and no overview pack has been rendered on device. No full
planet archive has been generated, and no on-device global-render claim is
made. Size/embedding analysis lives in
`ORCMAPS_EMBEDDED_WORLD_FIRMWARE_CONCEPT.md` (PLANNED/DEFERRED).

A future Pack Service may distribute or generate immutable pack triplets. It
is optional provisioning only:

```
optional internet -> provisioning/update -> local immutable packs -> OrcMaps
```

An installed pack never depends on a remote catalog, authentication, license
server, attribution lookup, telemetry endpoint, update check, or remote tile
server.
