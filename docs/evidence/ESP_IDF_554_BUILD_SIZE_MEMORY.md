# ESP-IDF 5.5.4 build, ESP32-P4 code size, and memory audit

**Scope:** release 0.2.0. `main` at `c91ff19` (streaming decode,
`LocationPicker`, `PartitionByteSource`, `provision_pack.py`) plus the
release fixes on top of it. Measured 2026-09-24 in a Linux cloud container.
The first round of measurements, on `8784b9e` before streaming, is kept at
the end for comparison.

Nothing here was run on hardware. Heap figures are **host x86-64**; they
show structure, not ESP32 numbers.

## Toolchain

- ESP-IDF **v5.5.4** (git tag), `riscv32-esp-elf-gcc` esp-14.2.0_20260121.
- The `espressif/idf:v5.5.4` Docker image could not be pulled (Docker Hub
  429). ESP-IDF was installed natively. `dl.espressif.com` was unreachable, so
  the Python environment was installed without its constraints file; this
  affects only Python tool versions, not the compiler or ESP-IDF sources.
- The component registry was unreachable. M5GFX **0.2.27**
  (`93b480bb349749202c8a2a953065c8ae95f58320`) and M5Unified **0.2.20**
  (`774d920cd6851a5231748b56ece1b073645f313f`) were taken from their GitHub
  tags and added as local components with `IDF_COMPONENT_MANAGER=0`.

## Compile results (target esp32p4)

| Build | Result | Notes |
|---|---|---|
| `examples/generic-esp32` | **builds** | Unmodified. |
| `examples/m5gfx` | **builds** | Scratch copy; M5GFX from its GitHub tag. |
| `examples/m5stack-tab5` | **builds** | Scratch copy; M5GFX/M5Unified from their GitHub tags. |
| OrcSDR-shaped probe | **builds** | Calls every API in `docs/ORCSDR_INTEGRATION.md`. |
| Managed `git:` dependency, pinned by SHA | **resolves and builds** | `orcmaps: {git: https://github.com/hardcoreerik/orcmaps.git, version: <sha>}`, `REQUIRES orcmaps`. Lock file records `source.type: git`. |

No compiler warnings came from OrcMaps sources. The remaining warnings are
in ESP-IDF headers included by M5GFX.

In-repo examples use the checkout folder name as the component name
(`REQUIRES OrcMaps`), so a checkout named `orcmaps` on a case-sensitive file
system fails to configure them. A managed dependency is not affected.

## Code size: OrcMaps component on ESP32-P4

`esp_idf_size --archives`, `libOrcMaps.a` only. The linker drops unused code,
so the figure depends on what the application calls.

| Image | Optimization | Flash `.text` | Flash `.rodata` | Total flash | Internal `.bss` |
|---|---|---:|---:|---:|---:|
| **OrcSDR-shaped probe** | `-O2` | 79,518 B | 1,367 B | **80,885 B** | 208 B |
| **OrcSDR-shaped probe** | `-Os` | 43,324 B | 1,386 B | **44,710 B** | 208 B |
| `examples/m5stack-tab5` | `-O2` | 79,978 B | 1,339 B | 81,317 B | 416 B |
| `examples/m5stack-tab5` | `-Os` | 43,376 B | 1,358 B | 44,734 B | 416 B |
| `examples/m5gfx` (calls almost nothing) | `-O2` | 5,290 B | 220 B | 5,510 B | 208 B |

The probe uses:

- `PartitionByteSource`, `PmTilesReader::StreamTile` and `LocationPicker`
  for the first-boot world picker;
- `esp_idf::PackFileSystem`, `DiscoverPacks`, `ResolvePack` and
  `FileByteSource` for the SD packs;
- `ClearMapBackground`, `RenderFeatureAt`, the experimental classifier and
  the M5GFX `DisplayTarget`.

Its `main` is 8.7 KB at -O2, including the header-only M5GFX adapter.

The z0-z4 world pack (871,343 B) is data and does not count here. It does
not fit OrcSDR's ~470 KB free app space, so it needs its own data
partition.

## Memory audit (streaming pipeline)

OrcMaps allocates only through `new`/`malloc`. It never calls
`heap_caps_malloc` and never asks for internal or DMA memory. ESP-IDF's
`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL` therefore decides which heap each block
comes from.

Stack: before this release, `DecompressPayload`, which is used for PMTiles
directory reads, put miniz's `tinfl_decompressor` on the calling task's
stack: 8,364 B on RV32 (`sizeof` under riscv32-esp-elf-gcc). 0.2.0 moves it
to the heap, as the streaming inflater already did.

Host heap profile: valgrind DHAT, one 1280x720 frame through
`PmTilesReader::StreamTile` + `RenderFeatureAt`, scratch reserved with 64 KiB
for features. It counts blocks allocated from OrcMaps code that are live at
the process peak, excluding the host framebuffer and the test's file buffer.

| Frame | Pipeline heap live | Blocks < 8 KiB | Blocks ≥ 8 KiB |
|---|---:|---:|---|
| Springfield z14 (`examples/m5stack-tab5/test-pack`), 15 of 24 tiles present | ~344 KB | ~2,800 blocks, ~163 KB | 98,304 (ring vector), 41,207 (feature bytes), 32,768 (window), 8,376 (inflater state) |
| World z0-z4 pack at z2, 16 tiles | ~121 KB | 41 blocks, ~14 KB | 65,536 (reserved feature buffer), 32,768, 8,376 |

The 0.1.0 batch path held **~2.35 MB** for the same Springfield frame
(~28,200 small blocks). The large buffers are reserved once and reused, and
at `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=8192` they go to PSRAM. The small
per-feature blocks go to internal RAM first. That is the figure to watch
inside OrcSDR.

## World z0-z4 pack

`tools/pack-builder/build_world_overview.py`, with Planetiler 0.10.2,
go-pmtiles 1.28.2 and Java 21.0.10, was run on Linux against Natural Earth
5.1.2 acquired with `acquire_world_overview_sources.py`. The z4, z5 and z6
archives are **byte-identical** to the ones recorded in
`WORLD_OVERVIEW_HOST_MEASUREMENTS.md`, which were built on Windows:

| Archive | Bytes | SHA-256 |
|---|---:|---|
| z0-z4 | 871,343 | `9aea08772bacf1f024d1da90cc52aa8fcf0b0e37405415dc7c91e75e56596f0f` |
| z0-z5 | 1,492,862 | `6f5c37f6cb505315e4c8a1d74efcf634fdb547b59128422ebf71c8ecf99addcc` |
| z0-z6 | 4,833,728 | `ae653cd1f422681a0fe4a401b1e7a914208983ff4822304aaad03dffd1b22241` |
| z0-z7 | 9,737,500 | `a6942c11782eb843235bbfdf78de89de0c6fea25c9a5a37abb67aab5cca4028c` |
| z0-z8 | 17,165,758 | `fbebdddd333221fd862c6c71dc4b40d434b07fefee6838a53ea7a210e0f8cb66` |

The z4 checks:

- `tools/pack-verify` installs it as `world`: class `clean`, no required
  credits, manifest sources `natural-earth` only.
- `tools/embedded-world-check` reads the archive inside an 0xFF-padded
  917,504-byte partition image, the way `PartitionByteSource` exposes flash.
  `PmTilesReader` opens it, and `LocationPicker` frames it at 1280x720,
  930x720 and 480x320 (opening zoom z2, z2 and z1). Each run streams the
  opening view, a pick at Springfield, Oregon zoomed to z4, and a half-screen
  drag. **Every visible tile was present and decoded; none missing, none
  failed.**
- Rendering shows a pre-existing defect: some tiles appear as lighter
  patches with unfilled land, and boundaries are visible only in those
  patches. The batch renderer (`pack-inspect preview`) produces the same
  image, and so did `8784b9e` before streaming. The renderer fills polygons
  from their first ring only; see `docs/ORCSDR_INTEGRATION.md` section 8.

## Earlier round: `8784b9e`, before streaming

- `libOrcMaps.a` in `examples/m5stack-tab5`: 80,355 B flash at -O2, 43,110 B
  at -Os. Streaming, discovery and the picker added about 1 KB at -O2
  (Tab5 image).
- Host heap for the Springfield z14 frame with the batch decoder: ~2.35 MB
  live, ~28,200 blocks under 8 KiB (~1.73 MB), 15 blocks of 8 KiB or more
  (~0.61 MB).
