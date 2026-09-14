# OrcMaps M5Stack Tab5 hardware demo

Standalone first-party reference: the same Springfield / 97477 PMTiles
archive that the host renderer used, from a removable SD card, through
OrcMaps, onto the Tab5 1280×720 panel.

This is **not** OrcSDR. OrcSDR is only the known-good board-bring-up
reference (M5Unified + SDMMC Slot 0).

## Hardware required

- M5Stack Tab5 (ESP32-P4)
- USB-C for flash / USB Serial/JTAG
- microSD card (FAT32 recommended) with the Springfield pack

## Tested / pinned versions

| Piece | Version | Why |
|---|---|---|
| ESP-IDF | **5.5.x** (`>=5.5.0,<5.6.0`) | OrcSDR's hardware-proven Tab5 environment |
| M5Unified | **0.2.20** | OrcSDR `apps/orcsdr-tab5/main/idf_component.yml` |
| M5GFX | **0.2.27** | Same pair as OrcSDR / `examples/m5gfx` |
| OrcMaps | this repository, as a component | `EXTRA_COMPONENT_DIRS` = repo root |

OrcMaps core still compiles under ESP-IDF 6.0.2 (`examples/generic-esp32`,
`examples/m5gfx`). This demo pins 5.5.x **only** so Tab5 M5Unified bring-up
matches the device that already works.

## Display bring-up

Same as OrcSDR:

```
auto config = M5.config();
M5.begin(config);
M5.Display.setRotation(1);
M5.Display.setBrightness(180);
```

Expected panel after rotation: **1280×720**.

No custom MIPI init, no custom LovyanGFX board class, no PMIC rewrite.

## SD hardware (Slot 0 only)

Native SDMMC Slot 0, 4-bit, `SDMMC_FREQ_HIGHSPEED`, LDO channel 4:

| Signal | GPIO |
|---|---|
| CLK | 43 |
| CMD | 44 |
| D0 | 39 |
| D1 | 40 |
| D2 | 41 |
| D3 | 42 |

Mount point: `/sd`.

ESP-Hosted / C6 / Slot 1 are **not** started. OrcSDR's
`hosted_transport_ready` / `sdmmc_host_init_already_running` stubs are
**not** copied.

Writable-file `_IONBF` (OrcSDR `setvbuf(..., _IONBF, 0)`) is **not**
copied. `FileByteSource` keeps the file open and uses default libc
buffering + `fseeko`/`fread`.

## SD card layout

Format the card FAT32 (or exFAT if your IDF FatFS build supports it).

Copy the host-generated pack:

```
data/local/springfield-97477.pmtiles
```

to the card as:

```
/orcmaps/springfield.pmtiles
```

After mount the firmware opens:

```
/sd/orcmaps/springfield.pmtiles
```

Do not embed the archive in firmware. Do not commit it to git.

Host regeneration (from repo root, Java 21 + Planetiler):

```
pwsh tools/pack-builder/build_springfield_pack.ps1
```

## Demo map (application constants, not core defaults)

| | |
|---|---|
| center | 44.0500 N, 123.0220 W |
| zoom | 14 |
| tile size | 256 px |
| style | `orcsdr-dark` |
| decompress budget | 512 KiB (demo policy) |

Expected tile counts: **24 visible / 20 present / 4 missing** (pack bbox).
Missing tiles are skipped, not errors.

## Build / flash / monitor

Use ESP-IDF **5.5.x** (example: `C:\Espressif\frameworks\esp-idf-v5.5.4`).

```
. C:\Espressif\frameworks\esp-idf-v5.5.4\export.ps1
cd examples/m5stack-tab5
idf.py set-target esp32p4
idf.py build
idf.py -p COMx flash
idf.py -p COMx monitor
```

Replace `COMx` with the Tab5 USB-Serial/JTAG port.

## Expected first boot

1. Black screen, then a short status list:
   - Display .... 1280x720 OK
   - PSRAM ...... OK
   - SD ......... OK
   - Map ........ OK
   - SD read benchmark...
   - Rendering Springfield...
2. Map fills in tile by tile (OrcSDR Dark, thick roads, no POI dots).
3. Completed map **stays on screen**. No pan/zoom loop.
4. Serial log ends with `ORCMAPS TAB5 HARDWARE DEMO` and `RESULT: PASS`.

On failure the screen keeps a red error line and the USB log has the
`esp_err` / path. It does not reboot-loop.

## Serial numbers to copy back

Please send the USB-Serial log containing:

- `sdmmc_card_print_info` (card / bus / frequency)
- every `ORCMAPS_SD_READ` line (block size, bytes, ms, MB/s)
- `ByteSource bytes_read=...`
- timing line (`GetTile` / gzip / decode / translate / classify / render / total)
- memory line (internal + PSRAM before/min)
- busy-sample internal/PSRAM around GetTile → decompress → MvtTile → FeatureTile → render → release
- `RESULT: PASS` or the FAIL line

Label those **ESP32-P4 / TAB5 HARDWARE RESULTS**, not host numbers.

## Troubleshooting

| Symptom | Check |
|---|---|
| Display not 1280×720 | Rotation/M5Unified version; this demo requires 0.2.20 |
| PSRAM not initialized | `sdkconfig.defaults` SPIRAM flags; Tab5 PSRAM |
| SD mount failed | Card seated, FAT32, Slot 0 pins, LDO4 |
| Map missing | Path `/orcmaps/springfield.pmtiles` on the card |
| Open failed | File truncated / not the gzip PMTiles from pack-builder |
| Alloc crash mid-tile | Send heap/PSRAM mins; do not "fix" with a cache yet |

## What this example is allowed to depend on

- OrcMaps, M5Unified, M5GFX, ESP-IDF SDMMC/FAT/VFS

OrcMaps **core** still has `REQUIRES ""` and no M5Unified. The M5GFX
adapter still has no Tab5 or M5Unified knowledge.
