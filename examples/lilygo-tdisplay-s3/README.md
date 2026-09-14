# OrcMaps LilyGO T-Display-S3 Touch hardware demo

Standalone Springfield map test for the LilyGO T-Display-S3 Touch with
the SD Shield fitted. It opens `/sd/orcmaps/springfield.pmtiles`, renders
the OrcSDR Dark style at zoom 14, and prints SD, timing, and memory
measurements over USB serial.

## Status

- ESP-IDF 5.5.4 build: **PASS** (2026-09-14)
- Host OrcMaps regression test: **PASS**
- Flash / display / SD / map render: **pending physical verification**
- Touch input: **not used by this static render test**

## Hardware

| Part | Configuration |
|---|---|
| MCU | ESP32-S3R8, 16 MiB flash, 8 MiB OPI PSRAM |
| Display | ST7789, 8-bit parallel, 320×170 landscape |
| Board power | GPIO 15 high |
| SD Shield | 1-bit SDMMC: CLK 11, D0 12, CMD 13 |
| Map path | `/sd/orcmaps/springfield.pmtiles` |

The panel and SD pins come from LilyGO's official
[T-Display-S3 repository](https://github.com/Xinyuan-LilyGO/T-Display-S3).
The LovyanGFX panel configuration is derived from LilyGO's
MIT-licensed `ST7789_Handler.h`. M5GFX 0.2.27 supplies LovyanGFX and
OrcMaps' existing `DisplayTarget`; no new OrcMaps render adapter is
needed.

## Map card

The same public Springfield / 97477 test pack used by the Tab5 demo is
stored at:

```
examples/m5stack-tab5/test-pack/springfield-97477.pmtiles
```

Copy it to `/orcmaps/springfield.pmtiles` on a FAT32 microSD card. The
firmware never formats or writes the card.

## Build, flash, and monitor

Use ESP-IDF 5.5.x:

```powershell
. C:\Espressif\frameworks\esp-idf-v5.5.4\export.ps1
cd examples/lilygo-tdisplay-s3
idf.py set-target esp32s3
idf.py build
idf.py -p COM32 flash
idf.py -p COM32 monitor
```

If automatic reset cannot enter the ROM downloader, hold BOOT, tap
RESET, release BOOT, and retry the flash command.

## Pass boundary

The build alone proves only ESP32-S3 compilation. Physical PASS requires:

1. Display reports 320×170.
2. PSRAM initializes.
3. The SD Shield mounts and prints card information.
4. The Springfield archive opens and renders.
5. Serial ends with `ORCMAPS LILYGO T-DISPLAY-S3 HARDWARE DEMO` and
   `RESULT: PASS`.

The map remains on screen. Touch gestures, pan, and zoom are deliberately
outside this first static test.
