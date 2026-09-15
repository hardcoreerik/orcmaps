# Tab5 interactive OrcMaps demo — hardware evidence

**This is a real physical M5Stack Tab5 result, not a host simulation.**
Status: **hardware-verified interactive demo**. Two runs recorded below: the
first exposed a framing bug, the second confirms the fix.

## Device and build

| | |
|---|---|
| Device | M5Stack Tab5 |
| MCU | ESP32-P4 rev v1.3, 32 MiB PSRAM, 360 MHz |
| Display | ST7123, 1280x720 (M5GFX autodetect `board_M5Tab5`) |
| Touch | ST touch FW 03 |
| SD card | SD32G, SDHC, 4-bit bus, 40 MHz, 29820 MB |
| ESP-IDF | v5.5.4 |
| Firmware | `orcmap_m5stack_tab5.bin`, 635,360 bytes (39% of app partition free) |
| Flash port | COM17, all image hashes verified by esptool |

## What ran

Boot reached the interactive map, not a benchmark menu. The world overview
pack was **not** present on the card for this run, so the demo started on the
installed regional pack instead of dead-ending — see "Known gaps".

## Measured interactive frame (first run, hardcoded framing)

Emitted to serial as JSONL while the demo was simply being used (not a
benchmark action). Single record, verbatim values:

| Field | Value |
|---|---:|
| pack | `springfield-97477` / `openmaptiles-3.16` |
| zoom | 15 |
| center | 44.0500, -123.0220 |
| viewport | 1280 x 600 |
| tiles visible / present / missing | 18 / 18 / **0** |
| features | 1,764 |
| bytes stored | 419,604 |
| bytes decompressed | 636,047 |
| lookup | 270.556 ms |
| inflate | 68.033 ms |
| decode | 471.467 ms |
| translate | 114.619 ms |
| classify | 3.051 ms |
| render | 352.361 ms |
| **frame total** | **1,378.504 ms** |
| internal heap free before / after | 401,175 / 400,055 |
| internal heap min / largest block | 145,167 / 253,952 |
| PSRAM free before / after | 29,592,564 / 29,592,564 |
| PSRAM min / largest block | 29,355,996 / 29,360,128 |
| result | PASS |

## Framing correction (second hardware run)

The first run used a hardcoded centre of 44.0500, -123.0220 -- a Springfield
*city* coordinate, **not** the extract's centre of 44.06000, -123.00750. The
view was therefore offset 169 px west and 162 px south, which at z14 on 1280
px piled 256 px of empty space onto the left edge, left 0 px on the right, and
pushed 82 px of usable pack data off-screen. The user reported the zoom, size,
and map quality as passing but the framing as "not quite centered"; that
report was accurate.

Fixed by deriving framing in the engine instead of hardcoding it. `FitBounds`
already existed but was never called, and alone it selects z13 here (it frames
the *entire* extract, which is height-constrained: 15 of 24 tiles empty, 2,537
ms). `orcmap::FillBounds()` was added as its complement -- smallest zoom whose
viewport is fully covered by the bounds -- and the demo prefers it, falling
back to `FitBounds`.

Second run, same device and card:

| Field | Value |
|---|---:|
| centre | **44.060008, -123.007500** (extract centre; offset now 0) |
| zoom | **15**, derived from 1280x600 |
| tiles visible / present / missing | 18 / 18 / **0** (no empty margin) |
| features | 1,394 |
| lookup / inflate / decode | 275.721 / 70.751 / 449.296 ms |
| translate / classify / render | 114.939 / 2.406 / 286.870 ms |
| **frame total** | **1,299.126 ms** |
| internal min / largest | 145,019 / 253,952 |
| PSRAM min / largest | 29,352,916 / 29,360,128 |
| result | PASS |

The example now supplies only `width_px`/`height_px`/`tile_size_px`; centre and
zoom are both engine-derived. Host tests assert that the same pack framed on
1280x600 and 320x170 yields the same centre and different zooms.

Partial coverage is now rendered and labelled `(partial coverage)` rather than
refused. An earlier iteration showed a blocking `DETAIL NOT INSTALLED FOR THIS
VIEW` message at z14, which discarded a view that was 86.5% real data -- worse
than the original demo. That was reverted.

## Comparability warning

This is **not** directly comparable to the earlier ~4.4 s (previously 11.3 s)
Tab5 Springfield figure. Two inputs changed at once:

- zoom is 15, not 14;
- the map viewport is 1280x600, not 1280x720, because the demo reserves a
  44 px top bar and a 76 px bottom bar.

The z14 view is retained as its own benchmark scenario precisely so the
historical number stays reproducible. Do **not** present 1,378 ms or 1,299 ms
as an optimization of 4,400 ms.

## Coverage finding

`ResolvePack()` requires full geographic coverage, and a 1280x600 z14
viewport is **wider than the Springfield extract**, so that pack is correctly
ineligible for *automatic selection* at z14. It first covers a full viewport
at z15 on this display, which is what `FillBounds` derives here -- z15 is a
computed result of 1280x600, not a hardcoded demo constant, and a 320x170
display resolves to a different zoom from the same pack.

Two refinements followed from this:

- automatic selection (`ResolvePack`, strict `Contains`) is unchanged, so a
  small extract can never be silently promoted to the global basemap;
- the demo additionally renders a pack that merely *overlaps* the view,
  labelled `(partial coverage)`, so manually zooming out to z14 still shows
  the 86.5%-covered map rather than a blocking message.

The z14 view also survives as a measurement-only scenario, where benchmark
scenarios bind their pack explicitly rather than relaxing any coverage rule.

## Evidence boundary

| Gate | Result |
|---|---|
| ESP-IDF 5.5.4 build (esp32p4) | PASS |
| Flash + image hash verification (COM17) | PASS |
| Boot to interactive map | PASS |
| SD mount and pack open | PASS |
| Measured frame emitted (serial + numbered SD JSONL) | PASS |
| Full-coverage render at engine-derived zoom | PASS (18/18 tiles, z15) |
| Centre matches extract centre | PASS (44.060008, -123.007500) |
| Partial-coverage rendering + label | IMPLEMENTED, NOT YET PHOTOGRAPHED |
| Touch pan / zoom / style / info physically exercised | NOT YET RECORDED |
| World -> regional transition on device | NOT YET RECORDED (world pack absent) |
| Benchmark action (`Run Benchmarks`) on device | NOT YET RECORDED |

Numbered reports are written to `/sd/orcmaps/orcmaps-benchmark-NNNN.jsonl`
and never overwritten; this session advanced through 0007.

## Known gaps

- The world overview pack was not on the card, so the world-first boot
  experience and the world -> regional story are unverified on hardware.
  Copy the built z0-7 candidate to `/orcmaps/world-overview.pmtiles`.
- `build_world_overview.py` writes bounds as `85.0511288`, which
  `ValidBounds()` rejects (it requires <= `85.05112878`). The demo compiles
  in the engine constant instead; `llround(x * 1e7)` still yields
  +/-850511288 so `pack_id` identity is unchanged. **The builder rounding
  must be fixed before runtime manifest JSON discovery can load these
  packs.**
- No decoded/rendered tile cache exists. Drag uses a presentation-layer
  offset blit of the finished PSRAM canvas; the pipeline re-runs on release.
- Text labels, polygon holes, overzoom, and pinch zoom remain unimplemented.
