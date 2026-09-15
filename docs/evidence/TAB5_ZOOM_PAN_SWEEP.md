# Tab5 zoom/pan sweep — rate measurement and optimization

**Real physical M5Stack Tab5 results.** Four runs on the same device, same
SD card, same three installed packs (world z0-7, oregon z1-13, springfield
z0-15).

## What the sweep does

Every zoom level from the deepest installed detail (z15) down to the
display's own floor (z2) and back up — 28 level-runs. At each level it
re-centres on the deepest pack, then pans **5 full viewport widths right and
5 back left**, rendering every step: 11 frames per level, 308 attempted
frames per sweep.

It calls `RenderMapFrame()`, the same function an interactive redraw calls,
so the numbers describe what the demo actually does. `fps` is **uncached
pipeline throughput** (1000 / mean frame ms), not a smoothed animation rate:
there is no tile cache, so every frame is a full re-render.

Run it from the demo (Info → Sweep), or build the measurement-only variant
that runs it at boot so it can be captured without a human tapping:

```
idf.py -DORCMAP_TAB5_AUTOSWEEP=1 build
```

## Results

Frame-weighted means reconstructed from the 28 per-level records of each
run, computed identically for all four so they are comparable.

| Run | frames | mean ms | fps | lookup | decode | render | translate |
|---|---:|---:|---:|---:|---:|---:|---:|
| 1. baseline | 308 | 1260.8 | 0.793 | 370.5 | 275.6 | 412.0 | 103.3 |
| 2. control (measurement fixed) | 250 | 1188.5 | 0.841 | 369.8 | 243.7 | 424.5 | 73.5 |
| 3. **+ leaf-directory cache** | 250 | **936.4** | **1.068** | **118.8** | 243.4 | 423.3 | 73.5 |
| 4. + stroke run coalescing | 250 | 932.0 | 1.073 | 119.0 | 243.6 | 418.4 | 73.5 |

Runs 2–4 are a controlled comparison: **identical workload** — 250 measured
frames, 4,872 distinct tiles, 268,116 features, 35,615,886 stored bytes in
every run. Run 1 is not comparable, see below.

### A measurement bug had to be fixed first

Run 1 counted 308 frames and 6,072 tiles; runs 2–4 count 250 and 4,872. The
difference is not a workload change — it is a **bug in the measurement**.
When a frame had no installed coverage the demo drew "DETAIL NOT INSTALLED
FOR THIS VIEW" and returned *without* running the pipeline, leaving
`g_last_frame` holding the **previous** frame's statistics, which the sweep
then counted a second time. 58 such frames were double-counted.

Fixed by clearing the frame record on that path and counting no-coverage
frames separately (`no_coverage_frames`). Run 1's figures are therefore
retained only as a record of what was first observed; **run 2 is the true
baseline.**

Those 58 blank frames are legitimate: panning 5 screens at z15 genuinely
leaves a 0.095° × 0.06° pack's coverage. They are excluded from the rate
because a frame that renders a message is not a pipeline measurement, and
including them would make the demo look faster than it is.

### What worked: leaf-directory cache (run 2 → 3)

`PmTilesReader` cached the root directory but re-read **and re-inflated** the
relevant leaf directory from storage **on every single tile lookup**. A frame
of 16–24 tiles mostly hits one or two leaf directories, so nearly all of that
work was redundant.

A four-slot round-robin cache keyed by (offset, length):

- `mean_lookup_ms` **369.8 → 118.8 (−67.9%)**
- `mean_frame_ms` **1188.5 → 936.4 (−21.2%)**
- `fps` **0.841 → 1.068 (+27.0%)**
- sweep wall time 496.5 s → 432.3 s

Correctness: identical tiles, features and bytes in both runs, and the host
suite — which verifies PMTiles lookups byte-exact against a reference-built
fixture including leaf-directory indirection — passes unchanged.

### What did NOT work: stroke run coalescing (run 3 → 4)

`orcsdr-dark` draws motorways at 3 px and primaries at 2.5 px, and every
width > 1 px went through a square-brush rasterizer that issued **one
`FillRect` per Bresenham pixel**. Coalescing axis-aligned runs into a single
rectangle per run collapses a long horizontal 3 px road from 117 rect calls
to 1.

The optimization is real, pixel-identical (a host test compares it against
the per-pixel reference over 84 line/width combinations, including
partially-offscreen and diagonal cases) and never issues more calls than
before. **It produced no measurable speedup:** render 423.3 → 418.4 ms, a
1.2% change that is within run-to-run noise, and the overall frame mean moved
0.5%.

So the hypothesis that thick-line brush calls dominated render time was
**wrong**. It is kept because it is strictly fewer calls for identical output
and is now test-locked, but it must not be presented as a performance win.
Render remains ~418 ms and is now the largest single stage; where it actually
goes is unmeasured, and polygon fill rather than line stroking is the next
thing to instrument.

## Per-level detail (run 3, descending)

| zoom | fps | mean ms | present | missing | lookup | decode | render |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 15 | 2.21 | 453 | 90 | 108 | 94 | 143 | 120 |
| 14 | 0.28 | 3570 | 165 | 33 | 535 | 1101 | 719 |
| 13 | 0.46 | 2156 | 264 | 0 | 651 | 643 | 562 |
| 12 | 0.70 | 1435 | 198 | 0 | 432 | 311 | 500 |
| 11 | 0.96 | 1038 | 198 | 0 | 409 | 137 | 398 |
| 10 | 0.76 | 1310 | 216 | 48 | 533 | 239 | 388 |
| 9 | 0.55 | 1810 | 180 | 84 | 563 | 426 | 535 |
| 8 | 0.98 | 1022 | 112 | 152 | 405 | 209 | 278 |
| 7 | 2.54 | 393 | 198 | 0 | 239 | 19 | 112 |
| 3 | 0.55 | 1831 | 264 | 0 | 406 | 300 | 981 |
| 2 | 1.66 | 603 | 132 | 0 | 172 | 80 | 300 |

(Values above are from the pre-cache run and show the *shape* of the cost:
z14 is the worst level at 0.28 fps because it has the most present tiles with
the densest geometry. z15 looks fast only because half its frames had no
coverage.)

## Honest limits

- One device, one card, one style (`orcsdr-dark`). Not a cross-board result.
- `fps` is uncached throughput. A tile cache — still unimplemented — would
  change these numbers more than any of the above, since panning re-fetches
  and re-decodes tiles it just drew.
- The sweep's deepest levels leave pack coverage by design; always read
  `no_coverage_frames` and `tiles_missing` next to a rate.
- Run 4's device-emitted `sweep_summary` line was lost to a serial buffer
  overrun; its figures are reconstructed from the surviving 28 per-level
  records, which is how every row in the table above was computed.
