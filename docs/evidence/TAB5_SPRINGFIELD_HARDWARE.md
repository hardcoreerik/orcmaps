# Tab5 Springfield hardware evidence

**This is a real physical M5Stack Tab5 result, not a host simulation.**

Firmware checkpoint that first ran on device: `1427597`. This document
records the PASS from that run. Status: **hardware-verified static
real-map render**. Not production-ready. No pan/zoom, no labels, no
navigation.

## Device / versions

| | |
|---|---|
| Device | M5Stack Tab5 (ESP32-P4) |
| Display | 1280×720 |
| ESP-IDF | 5.5.4 |
| M5Unified | 0.2.20 |
| M5GFX | 0.2.27 |
| Firmware | `1427597` |
| Result | **PASS** — map remained on screen |

## Map / viewport

| | |
|---|---|
| Archive | `/sd/orcmaps/springfield.pmtiles` |
| Public copy | `examples/m5stack-tab5/test-pack/springfield-97477.pmtiles` |
| Size | 3,507,636 bytes |
| SHA-256 | `8bf23873915668f41d098b98df32b11a6d08ec6754a63d885fce2f29abe4adfd` |
| Center | 44.0500 N, 123.0220 W |
| Zoom | 14 |
| Tile size | 256 |
| Style | `orcsdr-dark` |
| Visible / present / missing | 24 / 20 / 4 |

Four missing tiles are pack bbox, not an OrcMaps failure.

## SD card negotiation

| | |
|---|---|
| Name | SD32G |
| Type | SDHC |
| Size | 29820 MB |
| Bus | 4-bit (DAT0–DAT3, Slot 0 — Tab5 microSD maximum width) |
| Speed | 40.00 MHz (limit 40.00 MHz) |
| Power | on-chip LDO channel 4 |

A diagnostic warned LDO voltage 0 was outside 500–2700 mV. The card still
mounted. Do not treat that warning as the proven cause of slow reads.

8-bit / 16-bit microSD is not possible on this board. PSRAM X16 and the
ESP32-C6 Slot 1 SDIO link are unrelated.

## Sequential SD READ (512 KiB each)

OrcMaps used buffered `fread`/`fseeko`. Not OrcSDR `_IONBF`.

| Block | Bytes | ms | MB/s |
|---|---|---|---|
| 4 KiB | 524288 | 303.7 | 1.73 |
| 16 KiB | 524288 | 302.5 | 1.73 |
| 32 KiB | 524288 | 302.3 | 1.73 |
| 64 KiB | 524288 | 302.3 | 1.73 |

Block size did not change throughput. ~1.73 MB/s is slow for 40 MHz 4-bit
SDHC. PMTiles range reads were ~1.66 MB/s (`ByteSource` 1,011,846 bytes
in 610 ms). Storage is not the dominant cold-frame cost.

## Engine timing (ESP32-P4 / TAB5)

Host numbers are comparative only, not interchangeable.

**BEFORE no-text layer filter** (firmware `1427597`):

| Stage | Tab5 ms |
|---|---|
| GetTile / ByteSource | 610 |
| gzip | 170 |
| MVT decode | 5612 |
| translate | 2693 |
| classify | 66 |
| render | 1246 |
| **cold frame** | **11302 (~11.3 s)** |

**AFTER no-text layer filter / CURRENT** (`b396c7d`):

| Stage | Tab5 ms |
|---|---|
| GetTile / ByteSource | 610 |
| gzip | 169 |
| MVT decode | 1296 |
| translate | 707 |
| classify | 9 |
| render | 1239 |
| **cold frame** | **4421 (~4.4 s)** |

Current uncached cost is ~4.4 s. Decode+translate ~2.0 s; render ~1.24 s;
storage ~0.61 s. The 11.3 s figure is historical.

## Memory

PSRAM total 32 MiB. One-tile streaming. No OOM.

| | Internal | PSRAM |
|---|---|---|
| Before render | ~405 KiB free | ~29.7 MiB free |
| Minimum | **~40 KiB** | ~28.1 MiB |
| After first FeatureTile | ~405 KiB | ~28.8 MiB |

Busy tile `14/2591/5952`: 76,730 stored, 122,459 decompressed, 2,071
features. MvtTile decode drove internal heap to ~40 KiB; FeatureTile lived
in PSRAM; memory recovered after release.

## Known limitations

- Static frame only (no touch, pan, zoom)
- No text labels
- 1-path polygons, experimental FeatureKind mapping
- Bounded pack (west edge may show background)
- SD ~1.73 MB/s left for a later FatFS/VFS investigation
- Overzoom unsupported
