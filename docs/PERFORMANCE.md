# Performance

Status: **placeholder.** No OrcMaps-specific performance measurements exist
yet — a host renderer exists, but there is no real map pack and no
on-device map rendering measurement. Do not treat host framebuffer tests
as embedded performance numbers.

## Why this is empty

Per `PROJECT_TRUTH.md` principle 12 ("measure performance, do not optimize
blindly") and `ROADMAP.md`, real numbers land here starting in Phase 2
(first tile decode+render) and Phase 3 (Lane County vertical slice), not
before. Writing speculative numbers here would violate this project's own
"do not mark work complete without evidence" rule.

## Reference numbers from prior art (not OrcMaps measurements)

Cited in full in `docs/FORMAT_DECISION.md` — repeated here only as the bar
to compare against once real numbers exist:

- `yuiseki/m5-cardputer-offgrid-tiny-map` / `m5-cardputer-meshtastic-map`
  (ESP32-S3, **no PSRAM**): z13 tile render 33s → 8.1s after optimization,
  z0 render 0.4s; ~95 KB free heap with the map component loaded; needs one
  32 KB contiguous scratch block; only works with Bluetooth disabled.
- OrcMaps' initial target (ESP32-P4 Tab5, **with PSRAM**, larger heap)
  should comfortably beat these numbers if the architecture is sound —
  that comparison is exactly what Phase 2/3 measurement should confirm or
  refute.

## What will be measured here, once measurable

Per the original project brief, at minimum: map archive open time, tile
lookup time, SD bytes read per tile, decompression time, decode time,
render time, peak working RAM, peak PSRAM, cache hit/miss rate, full
viewport initial render time, cached viewport render time, pan latency,
zoom latency — each with the exact command/build used to produce the
number, not just a bare figure.
