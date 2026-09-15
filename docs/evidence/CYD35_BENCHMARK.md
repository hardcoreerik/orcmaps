# CYD 3.5" (ESP32-3248S035) — hardware benchmark

**Real physical hardware result.** First OrcMaps run on a board with **no
PSRAM**, using the same three packs as the Tab5's card, so the two boards are
directly comparable on identical data.

## Board

| | |
|---|---|
| Board | "Cheap Yellow Display" 3.5", ESP32-3248S035 |
| MCU | **ESP32-D0WD-V3** rev 3.1, dual-core, 160 MHz |
| **PSRAM** | **none fitted** (D0WD, not D0WDR2) |
| Free DRAM at boot | ~176 KiB largest region (+111 KiB D/IRAM, 69 KiB IRAM) |
| Flash | 4 MB (2 MB app partition; firmware 468 KB, 78% free) |
| Display | ST7796 320x480, driven **landscape 480x320** |
| SD card | SD32G on SPI, its own card (not the Tab5's) |
| Port | COM34 (CH340) — auto-reset works, no BOOT/RESET holding needed |
| Graphics | LovyanGFX 1.2.29, pinned by commit `fb7cec2` |

Pin mapping is the configuration already proven on this hardware in the
NEONDRIVE firmware (`include/variants/cyd35/cyd35_hw_config.h`), not a guess:
display on HSPI (MISO 12, MOSI 13, SCLK 14, CS 15, DC 2, RST tied high,
backlight 21 + 27), SD on a separate VSPI bus (SCLK 18, MISO 19, MOSI 23,
CS 5).

## Headline: the fps number is an artifact, read the skip column

| | |
|---|---:|
| frames measured | 296 |
| mean frame | **522.4 ms** |
| **fps** | **1.914** |
| wall time | 161.6 s |
| tiles visible | 1,880 |
| tiles **rendered** | **825 (43.9%)** |
| tiles **skipped, out of memory** | **1,035 (55.1%)** |
| tiles missing from packs | 20 (1.1%) |
| features drawn | 28,126 |

Counters reconcile exactly: 825 + 20 + 1,035 = 1,880.

**The CYD is not faster than the Tab5 — it is doing far less work.** Against
the Tab5's optimized run on the same packs:

| | Tab5 | CYD 3.5" |
|---|---:|---:|
| map area | 1280x600 | 480x320 |
| mean frame | 932.0 ms | 522.4 ms |
| fps | 1.073 | 1.914 |
| tiles rendered | 4,872 | 825 |
| tiles skipped (memory) | 0 | 1,035 |
| features drawn | 268,116 | **28,126** |

The CYD reports ~1.8x the frame rate while drawing **9.5x fewer features**,
and over half its tiles were never decoded at all. Quoting "1.9 fps" without
the skip rate would be straightforwardly misleading.

## The real finding: z8-z14 renders nothing

Per zoom, descending. `present` is tiles actually rendered:

| zoom | fps | mean ms | present | skipped | features |
|---:|---:|---:|---:|---:|---:|
| 15 | 1.22 | 817 | 16 | 32 | 868 |
| 14 | 2.00 | 499 | **0** | 20 | **0** |
| 13 | 2.44 | 410 | **0** | 66 | **0** |
| 12 | 2.12 | 471 | **0** | 93 | **0** |
| 11 | 2.78 | 360 | **0** | 62 | **0** |
| 10 | 2.78 | 360 | **0** | 62 | **0** |
| 9 | 2.78 | 360 | **0** | 62 | **0** |
| 8 | 2.66 | 375 | **0** | 36 | **0** |
| 7 | 2.21 | 452 | 72 | 21 | 531 |
| 6 | 1.82 | 550 | 59 | 3 | 1,065 |
| 5 | 2.29 | 437 | 62 | 0 | 1,312 |
| 4 | 1.36 | 738 | 60 | 2 | 3,285 |
| 3 | 1.30 | 770 | 29 | 33 | 1,456 |
| 2 | 1.37 | 731 | 91 | 5 | 4,150 |
| 1 | 1.80 | 556 | 22 | 22 | 1,375 |

**Zooms 8 through 14 drew nothing at all** — every tile was refused for
memory — and they report the *highest* frame rates (up to 2.78 fps) for
exactly that reason. An empty frame is cheap.

That range is the entire Oregon regional pack (z1-13). So on this board, with
this pipeline, **the world overview is usable at z1-z7 and Springfield partly
at z15, while mid-zoom regional detail is not usable at all.**

## Why: the decoder materialises a whole tile

Root-caused from a decoded backtrace, not inferred:

```
abort() at __wrap___cxa_allocate_exception
  operator new
    std::vector<uint32_t>::push_back
      DecodeFeature      src/tiles/mvt_decoder.cpp:276
      DecodeLayer        src/tiles/mvt_decoder.cpp:405
      DecodeMvtTile      src/tiles/mvt_decoder.cpp:443
```

`DecodeMvtTile` builds the complete `MvtTile` in memory, then
`TranslateMvtToFeatureTile` deep-copies it into a complete `FeatureTile`. A
dense mid-zoom tile simply does not fit in ~150 KB alongside the SD stack,
LovyanGFX and FATFS. This is an architectural limit of the current pipeline
on a no-PSRAM device, not a tuning problem: raising a budget cannot fix it.

A streaming decode — emit features to the renderer as they are parsed instead
of materialising the tile — is what would make mid zooms work here. That is
not implemented.

## Three bugs found bringing this board up

1. **M5GFX carries no ST7796 driver.** M5Stack's fork ships only the panels
   they use. LovyanGFX is not in the ESP Component Registry either, so it is
   pinned by git tag *and* full commit SHA. The OrcMaps display adapter is
   written against `lgfx::v1::LovyanGFX&`, so it now prefers M5GFX when
   present and falls back to LovyanGFX — the M5Stack examples are unchanged.

2. **`no available dma channel`.** The classic ESP32 has exactly **two** SPI
   DMA channels and this board needs both; `SPI_DMA_CH_AUTO` on the display
   bus left none for SD, and the mount failed with `ESP_ERR_NOT_FOUND`. Fixed
   by assigning channels explicitly (display 1, SD 2).

3. **Reset loop on first frame** — the OOM above. With C++ exceptions
   disabled a failed allocation calls `abort()`, so the board bootlooped
   before emitting a single measurement. This board now builds with
   exceptions enabled and the shared pipeline catches `std::bad_alloc`
   (guarded by `__cpp_exceptions`, so the PSRAM boards keep exceptions off),
   recording `tiles_skipped_memory`. An out-of-memory tile became a
   measurement instead of a crash — which is the only reason this document
   has numbers in it.

A measurement defect was also found and fixed here: a tile fetched
successfully but abandoned mid-decode was counted as **both** present and
skipped, so the first run's totals over-counted by 240. The handler now undoes
the present count, and the corrected run's counters reconcile exactly.

## What this run does and does not show

- It **does** show the runtime pack discovery layer working unchanged on a
  second, very different board: 3 manifests found, 3 installed, 0 rejected,
  from the same card layout the Tab5 uses.
- It **does** show the board-agnostic harness measuring a non-M5, non-PSRAM
  target with no changes to the shared pipeline beyond the memory guard.
- It does **not** show a usable map application on this board. Rendering goes
  straight to the panel (no offscreen canvas is possible), so `render_ms`
  includes SPI transfer, and mid zooms produce nothing.
- No touch, no interaction: this example is a benchmark instrument, and the
  Tab5's drag-blit model cannot be ported to a board with no PSRAM.
- Single board, single card, single style (`orcsdr-dark`).
