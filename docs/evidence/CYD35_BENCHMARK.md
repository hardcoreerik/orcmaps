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

## Streaming pipeline (second attempt at "every tile loads")

The pipeline was rebuilt to stream, and the engine change is real and
verified — but **the goal is not yet met on this board.** Recorded here with
the measurements rather than as a success.

### What was built

Nothing materialises a whole tile any more:

- `MvtDecodeOptions::feature_sink` — the decoder hands each feature over as
  it is parsed, into one reused `MvtFeature`, and accumulates nothing.
- `TranslateMvtFeature` / `RenderFeatureAt` — per-feature translate and draw,
  reusing one `Feature` with its buffers retained.
- `DecompressStreaming` / `PmTilesReader::GetTileInflated` — the compressed
  payload is pulled from the archive in 2 KiB pieces, so compressed and
  inflated bytes are never resident together.

Host tests gate all of it: the streaming decode is asserted **byte-identical**
to the materialising decode (same feature sequence, geometry, attributes, and
layer context, on the real MVT fixture), proven to reuse a single `Feature`
object address and to accumulate nothing, to honour the layer filter, and to
abort rather than truncate when a sink refuses. Streaming inflate is asserted
byte-identical to fetch-then-inflate, including into a reused buffer, with an
undersized budget refused rather than truncated.

### Measured effect on the CYD

| build | world z1 | oregon z7 | springfield z13 |
|---|---|---|---|
| materialising | 2 of 4 tiles | 0 of 9 | 0 of 6 |
| streaming decode | **4 of 4**, 632 features | 0 of 9 | 0 of 6 |
| streaming decode + inflate | 2-3 of 4 (varies) | 0 of 9 | 0 of 6 |

Streaming decode alone fixed the world view outright. Adding streaming
inflate did **not** help further and made the world view unstable, which
points at the actual remaining limit.

### The remaining blocker, numerically

It is not total memory, it is **contiguous** memory:

| | |
|---|---:|
| free internal heap | 190,264 bytes |
| **largest free block** | **69,632 bytes** |
| oregon z7 worst tile, inflated | 111,366 bytes |
| springfield z13 worst tile, inflated | 77,380 bytes |

The pipeline still needs ONE contiguous buffer holding a whole inflated tile,
and the largest block available is 70-106 KiB depending on fragmentation —
below what the dense tiles require. Frame-to-frame the largest block shrinks,
which is why the world view degraded from 4 of 4 to 2 of 4 across runs.

Pre-reserving a 132 KiB buffer at startup was tried and made things **worse**,
not better: it claimed the largest block and left a 77,824-byte maximum
behind, after which every tile failed. That attempt is recorded because the
intuition ("reserve early while memory is clean") was wrong here.

Two suspects were measured and **ruled out**: the resident root directories
of all three packs total 3.6 KiB, and `sizeof(tinfl_decompressor)` is ~8 KiB
(it holds no LZ dictionary), so neither explains the fragmentation.

### The two ways to finish this

1. **End-to-end streaming parse** — parse the MVT while inflating, through a
   sliding window, so no whole-tile buffer exists at any point. Peak becomes
   the inflate window plus one feature, roughly 40 KiB, which fits with room
   to spare and would make *any* pack work on this board. This is the durable
   fix and is not implemented.
2. **CYD-sized packs** — these packs were built for a 1280x720 Tab5 at
   extent 4096, which is 16x the pixel resolution a 480x320 screen can show.
   A pack built for this display (coarser simplification, fewer layers,
   smaller extent) would produce tiles small enough to load today, with no
   engine change.

(1) is the better engineering answer; (2) is the faster route to a working
demo. They are not exclusive.

## CORRECTION: the memory figures above were optimistic

Every "free heap" and "largest block" number earlier in this document — and
in the commits that produced them — came from
`heap_caps_get_free_size(MALLOC_CAP_INTERNAL)`. **That metric is misleading
on this chip.** It counts the 69 KiB instruction-RAM region (and part of
D/IRAM) which `malloc` cannot hand out for a byte buffer. Re-probed with
`MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT`, which is what a `std::vector`
actually gets:

| | reported before | actually allocatable |
|---|---:|---:|
| free heap | 190,264 | **91,732** |
| largest free block | 69,632 | **47,104**, falling to 14,336 |
| minimum free seen | 126,860 | **16,208** |

So the board has roughly **half** the data memory I had been reporting. The
earlier conclusions about *which* tiles fail were right — they were measured
from behaviour — but the headroom analysis was wrong, and that changes the
answer.

## Why end-to-end streaming is not enough here

The streaming parser was built as planned (`mvt_stream.{hpp,cpp}`,
two-pass over `InflatingByteStream`) and is correct: host tests assert it
emits features byte-identical to the buffered decoder, attributes included,
which is the proof that two-pass table resolution works. It is a real
reduction and the Tab5 uses it too.

But its footprint against the corrected budget does not fit:

| component | bytes | note |
|---|---:|---|
| LZ77 window | 32,768 | fixed by DEFLATE; cannot be smaller |
| inflater state | ~8,400 | `tinfl_decompressor` |
| input chunk | 2,048 | |
| all-layer key/value tables | up to 24,423 | worst measured, Oregon z7 |
| one feature's encoded bytes | up to 36,259 | worst measured, Springfield z13 |
| **total** | **~104,000** | against **91,732** available |

And the largest single piece (36 KiB) sits right at the 47 KiB largest-block
limit, which then collapses to 14 KiB. Measured result: Oregon z7 and
Springfield z13 still render 0 tiles; world z1 renders 2 of 4.

Going further would mean streaming *within* a feature, and that does not
work either: a 36 KiB encoded feature decodes to roughly 9,000 points, which
is ~72 KiB of `MvtPoint` — **larger than its encoded form**. Polygon fill
needs the whole ring, so it cannot be drawn incrementally the way a
polyline could.

## Conclusion: the data is wrong for this board, not only the code

These packs were built for a 1280x720 Tab5 at extent 4096 — sixteen times
the coordinate resolution a 480x320 screen can show — and contain single
features of 36 KiB. No streaming strategy makes that fit in 91 KiB with a
47 KiB largest block.

The remaining path is a pack built for this display: coarser simplification,
fewer layers, lower extent. That is a data change, and it is the honest
answer rather than a shortcut — the engine work above stands on its own
(it is what makes the world view render at all, and it reduces Tab5 memory),
but "every tile loads on a CYD" needs tiles sized for a CYD.

## Breakthrough: the blocker was the directory, not the tile

After all of the above, the thing actually breaking mid-zoom tiles was found
by measuring the reader rather than the renderer.

**A PMTiles leaf directory in these packs holds 4,096 entries.** Serialized
that is ~21 KiB of delta-varint bytes; parsed into `std::vector<DirEntry>`
it is **98,304 bytes** — a 4.6x inflation, and the single largest allocation
the reader made. Worse, the leaf cache added earlier for Tab5 speed kept
**four** of those **per reader**, with three readers open.

Springfield stores everything in its root directory (141 entries, zero leaf
pointers), which is why it had always behaved differently from world and
Oregon.

Fixed by searching directories in their serialized form:
`FindEntryInSerialized` walks the columnar layout (ids, run lengths,
lengths, offsets) sequentially and keeps nothing — only one entry is ever
needed. The cache now holds decompressed *bytes* (~21 KiB) instead of parsed
entries (~96 KiB), and two slots instead of four.

Validated on real packs with real 4,096-entry leaf directories, not just the
root-only test fixture: 13 tiles across world and Oregon, streamed features
identical to the buffered decoder, 0 mismatched, 0 failed.

### Measured on the device

| | before | after |
|---|---|---|
| world z1 | 2 of 4 tiles | **4 of 4, 632 features, PASS** |
| oregon z7 | 0 of 9 | **5 of 9, 396 features** |
| springfield z13 | 0 of 6 | 0 of 6 |
| free heap | 91,732 | 129,492 |
| largest block | 47,104 | 45,056-65,536 |

The world view now renders every tile. The horizontal seam visible in the
earlier photograph — the z1 tile-row boundary, where the upper row died
before its ocean polygon landed — is gone.

## The next blocker, measured

Springfield z13 now reports `translate` and `render` time with `decode` and
`bytes` at zero: features were being drawn and then memory ran out
(minimum free 15,112 bytes). The cause is **per-feature decoded geometry**:

- a 36,259-byte encoded feature holds roughly 9,000 coordinate pairs;
- decoded to `MvtPoint` (8 bytes each) that is ~72 KiB;
- `TranslateMvtFeature` then makes a SECOND ~72 KiB copy as `Feature`.

So one dense feature costs ~144 KiB against ~129 KiB of free heap. No
amount of tile-level streaming addresses this, because the cost is inside a
single feature.

The clear next step is to remove the duplicate copy: render directly from
the decoded MVT geometry instead of translating into a parallel `Feature`
structure, which halves per-feature geometry memory at a stroke. Shrinking
the point representation (both types are two int32s; extents fit an int16)
would halve it again. Neither is done.
