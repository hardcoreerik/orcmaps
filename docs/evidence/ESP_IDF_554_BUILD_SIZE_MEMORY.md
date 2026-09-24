# ESP-IDF 5.5.4 build, ESP32-P4 code size, and memory audit

**Scope:** measurements taken on 2026-09-24 in a Linux cloud container, against
`main` at `8784b9e` plus two fixes (`<algorithm>` include in a host test, z4/z5
derivation in the world builder). This is **before** the LocationPicker,
PartitionByteSource, streaming-decode and provision-tool commits reached
GitHub, so none of those are included in any number below.

Nothing here was run on hardware. Heap figures are **host x86-64**; ESP32-P4
pointers and `size_t` are half as wide, so device figures will be lower but were
**not measured**.

## Toolchain

- ESP-IDF **v5.5.4** (git tag), `riscv32-esp-elf-gcc` esp-14.2.0_20260121.
- The `espressif/idf:v5.5.4` image could not be pulled (Docker Hub 429). ESP-IDF
  was installed natively instead. `dl.espressif.com` was unreachable, so the
  Python environment was installed without the constraints file. This affects
  only Python tool versions, not the compiler or ESP-IDF sources.
- The component registry was unreachable. M5GFX **0.2.27**
  (`93b480bb349749202c8a2a953065c8ae95f58320`) and M5Unified **0.2.20**
  (`774d920cd6851a5231748b56ece1b073645f313f`) were taken from their GitHub
  tags and added as local components with `IDF_COMPONENT_MANAGER=0`. The
  registry packages were not compared against these tags.

## Compile results (target esp32p4)

| Example | Result | Notes |
|---|---|---|
| `examples/generic-esp32` | **builds** | Unmodified. |
| `examples/m5gfx` | **builds** | Scratch copy: M5GFX from its GitHub tag instead of the registry. |
| `examples/m5stack-tab5` | **builds** | Scratch copy: M5GFX and M5Unified from their GitHub tags. |

No compiler warnings were emitted from OrcMaps sources; the warnings in the
logs come from ESP-IDF headers (`periph_ctrl.h`, `gpio_ll.h`) included by
M5GFX.

**Checkout folder name matters.** The in-repo examples use the repository
folder's name as the component name (`REQUIRES OrcMaps`), so a checkout named
`orcmaps` (a default `git clone` on a case-sensitive file system) fails with
`Failed to resolve component 'OrcMaps'`. This affects only the in-repo examples.
A component-manager consumer names the component by its dependency key.

## Code size: OrcMaps component on ESP32-P4

`esp_idf_size --archives` on the `examples/m5stack-tab5` image, which runs the
full pipeline (PMTiles, gzip, MVT decode, translate, style, render, pack
discovery). The linker drops unused code, so this is the cost of what that
demo calls.

| Optimization | `libOrcMaps.a` flash `.text` | flash `.rodata` | internal `.bss` | Total flash |
|---|---:|---:|---:|---:|
| `-O2` (`CONFIG_COMPILER_OPTIMIZATION_PERF`, the Tab5 example default) | 79,016 B | 1,339 B | 416 B | **80,355 B** |
| `-Os` (`CONFIG_COMPILER_OPTIMIZATION_SIZE`) | 41,752 B | 1,358 B | 416 B | **43,110 B** |

Not included:

- The M5GFX `DisplayTarget` is header-only, so it compiles into the
  consumer's own component.
- `libstdc++`/`libc`/`libm` code shared with the application.
- The z0-z4 world pack (871,343 B). It is data, and it is larger than
  OrcSDR's ~470 KB of free app-partition space, so it cannot go into the app
  image. It needs its own data partition (read with `PartitionByteSource`).

`examples/m5gfx` links only 5,510 B of OrcMaps, because it calls almost none of
the pipeline. Don't use it as the size figure.

## Memory audit

### Where the pipeline allocates

All pipeline memory comes from `std::vector` (the default allocator, so
`malloc`/`new`). OrcMaps never calls `heap_caps_malloc` and never asks for
internal or DMA memory. Main allocation sites:

| Site | What | Size driver |
|---|---|---|
| `PmTilesReader` root directory | `std::vector<DirEntry>` (24 B/entry on 32-bit) | root dir ≤ 16 KiB compressed |
| `PmTilesReader` leaf cache | 4 slots of `std::vector<DirEntry>` | leaf entries × 24 B each, held for the reader's lifetime |
| Tile read + `DecompressPayload` | stored bytes + decompressed bytes | tile size (Tab5 sweep: up to hundreds of KB) |
| `DecodeMvtTile` | features, geometry and tag vectors | thousands of small blocks per busy tile |
| `TranslateMvtToFeatureTile` | per-feature paths and property vectors | same |
| Renderer / `DisplayTarget::FillPolygon` | per-polygon `xy` and scanline crossings | ring size |
| `tinfl_decompress_mem_to_mem` (miniz) | `tinfl_decompressor` **on the calling task's stack** | **8,364 B** on RV32 (measured with `sizeof` under riscv32-esp-elf-gcc) |

### Host peak (valgrind DHAT, one 1280x720 frame, x86-64)

| Frame | Live heap at peak, excluding the host framebuffer | of which blocks < 8 KiB | blocks ≥ 8 KiB |
|---|---:|---:|---:|
| Springfield z14 (`examples/m5stack-tab5/test-pack`) | ~2.35 MB | ~28,200 blocks, ~1.73 MB | 15 blocks, ~0.61 MB |
| World z0-z4 pack at z2 | ~0.35 MB | ~2,800 blocks, ~0.23 MB | 2 blocks, ~0.12 MB |

The size split is approximate: blocks are grouped by allocation site, using
the site's average block size.

### What this means on an ESP32-P4 with little internal RAM

- ESP-IDF sends a `malloc` smaller than `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL`
  to internal RAM first, and falls back to PSRAM when internal RAM runs out. Most of
  the pipeline's heap use is **small blocks**, so a busy tile can fill
  internal RAM transiently before the fallback happens. That starves other
  users (Wi-Fi, DMA buffers) unless internal RAM is reserved.
- The Tab5 demo's verified configuration is `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=8192`
  plus `CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=40960`: the reserve keeps 40 KiB of
  internal RAM for explicit internal/DMA allocations.
- Large buffers (tile bytes, decompressed payloads, big leaf directories)
  already go to PSRAM under that configuration. No OrcMaps allocation is
  hard-wired to internal RAM.
- The miniz decompressor state (8,364 B) is on the stack. A task that renders
  maps needs roughly 16 KiB of stack; the Tab5 demo runs in a 16,384 B main task.

No core code was changed for this audit. A per-call heap choice would mean an
allocator parameter on every `std::vector` in the public types: that's an API
change, not a small one. Moving the miniz state from the stack to the heap is
small, but it touches the decode path, which is also being changed in the
streaming-decode commits that have not been pushed yet.

## World z0-z4 pack reproduction

`tools/pack-builder/build_world_overview.py` (Planetiler 0.10.2, go-pmtiles
1.28.2, Java 21.0.10, Natural Earth 5.1.2 acquired with
`acquire_world_overview_sources.py`) was run on Linux. The z4, z5 and z6
archives came out **byte-identical** to the ones recorded in
`WORLD_OVERVIEW_HOST_MEASUREMENTS.md`:

| Archive | Bytes | SHA-256 |
|---|---:|---|
| z0-z4 | 871,343 | `9aea08772bacf1f024d1da90cc52aa8fcf0b0e37405415dc7c91e75e56596f0f` |
| z0-z5 | 1,492,862 | `6f5c37f6cb505315e4c8a1d74efcf634fdb547b59128422ebf71c8ecf99addcc` |
| z0-z6 | 4,833,728 | `ae653cd1f422681a0fe4a401b1e7a914208983ff4822304aaad03dffd1b22241` |
| z0-z7 | 9,737,500 | `a6942c11782eb843235bbfdf78de89de0c6fea25c9a5a37abb67aab5cca4028c` |
| z0-z8 | 17,165,758 | `fbebdddd333221fd862c6c71dc4b40d434b07fefee6838a53ea7a210e0f8cb66` |

The z4 archive was checked on the host with the existing tools:

- `tools/pack-verify` installs it as `world` (class `clean`, no required
  credits).
- `tools/pack-inspect header` reads it as PMTiles v3, MVT, gzip, z0-4,
  341 addressed tiles.
- `tools/pack-inspect preview` renders a 1280x720 world view at z2 (16/16
  tiles found) and an Oregon view at z4 (24/24 tiles found).

It was not opened through `PartitionByteSource` or framed with
`LocationPicker`, because neither exists on the published `main` yet.
