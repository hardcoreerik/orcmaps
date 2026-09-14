# OrcMaps Embedded World Firmware Concept

**Status:** PLANNED / DEFERRED  
**Current priority:** Continue the SD-card map path and hardware benchmarking first.  
**Related projects:** OrcMaps, OrcSDR  
**Purpose:** Preserve the firmware-embedded world-map idea as an architectural direction without allowing it to distract from the current SD-card integration and test program.

---

## 1. Concept Summary

OrcMaps has now demonstrated that a complete low-zoom world overview can be extremely small.

Measured Natural Earth overview candidates:

| Coverage | Size | Current interpretation |
|---|---:|---|
| z0-z4 | 871,343 bytes (~0.83 MiB) | Strong firmware-embedded fallback candidate |
| z0-z5 | ~1.42 MiB | More space with limited added fallback value |
| z0-z6 | 4.61 MiB | Tiny SD-card world map |
| z0-z7 | 9.29 MiB | Recommended normal SD-card world overview |
| z0-z8 | 16.37 MiB | Optional; limited extra local usefulness from Natural Earth |

The key idea is to make a very small world map available even when no SD-card map pack is present.

Conceptually:

```text
Regional detail pack on SD
        ↓ preferred where available
World overview z0-z7 on SD
        ↓ fallback
Embedded world z0-z4 in firmware flash
        ↓ final fallback
No blank map
```

For OrcSDR this would mean that geographic context is always available for RF overlays such as ADS-B aircraft, LoRa nodes, tracks, bearings, weather products, received stations, or other future geospatial features.

---

## 2. Important Architectural Rule

The embedded map must be an **optional storage source**, not a special OrcSDR-only map engine.

OrcMaps should continue to operate through its generic source/rendering architecture:

```text
ByteSource
   ↓
PMTiles
   ↓
MVT
   ↓
Feature / FeatureTile
   ↓
FeatureKind
   ↓
MapStyle
   ↓
RenderTarget
   ↓
M5GFX / LVGL / host / future adapter
```

The embedded firmware map should therefore be exposed through a flash-backed `ByteSource` or equivalent generic random-access source.

The same PMTiles content should remain usable regardless of graphics adapter.

Do **not** make the embedded-world feature depend on:

- M5GFX
- M5Unified
- Tab5
- OrcSDR
- LVGL
- a specific display controller
- a specific map style

OrcSDR may choose to use the feature, but OrcMaps should remain a general embedded mapping engine.

---

## 3. Preferred Flash Layout

For OrcSDR, the preferred production design is a dedicated read-only map partition rather than compiling the PMTiles bytes directly into the application binary.

Conceptual layout:

```text
ESP32-P4 / Tab5 flash

┌──────────────────────────────┐
│ Bootloader / partition table │
├──────────────────────────────┤
│ NVS / PHY                    │
├──────────────────────────────┤
│ OrcSDR application           │
├──────────────────────────────┤
│ Coredump / system data       │
├──────────────────────────────┤
│ OrcMaps Embedded World       │
│ ~1 MiB partition             │
├──────────────────────────────┤
│ Remaining / future use       │
└──────────────────────────────┘
```

The exact partition size must be selected from the current OrcSDR flash layout at implementation time. Do not assume the current layout is unchanged.

### Why a separate partition is preferred

- The map does not consume the application partition budget.
- The map can persist while the application firmware is updated.
- The map can potentially be updated independently of the application.
- OrcMaps can random-access the PMTiles content without loading the full map into RAM.
- The firmware binary remains conceptually separate from immutable geographic assets.
- Normal OrcSDR releases do not need to rewrite the map when it has not changed.

A direct embedded C-array/linker blob may still be useful for an early proof, but it should not become the long-term design unless there is a compelling reason.

---

## 4. Persistence Rules

The embedded world map is a **read-only base-map asset**. It is not user storage.

User-created map information must never be stored inside:

- the application image
- the embedded-world partition
- immutable PMTiles packs

Future user map content may include:

- markers / pins
- RF sites
- repeaters
- interference locations
- LoRa node notes
- favorite monitoring locations
- tracks
- polygons / zones
- annotations
- saved views

These must be stored separately using persistent user storage such as NVS, SD card, or a future dedicated user-data partition.

Recommended conceptual separation:

```text
Firmware / application       replaceable
Embedded world map           replaceable read-only asset
SD map packs                 replaceable map data
User overlays / annotations  persistent user data
```

User overlays should be stored geographically, for example by latitude/longitude and metadata, so replacing a base map does not invalidate them.

Example conceptual marker:

```text
latitude: 44.0521
longitude: -123.0867
type: rf_marker
label: Interference source
```

### Update behavior target

A normal firmware update should preserve:

- user settings
- user markers
- saved locations
- tracks
- SD-card map packs
- the embedded world map unless the release explicitly replaces it

A full-chip erase may destroy flash-resident user data and is a different operation from a normal upgrade.

---

## 5. Map Source Priority

The desired long-term local-only selection order is:

```text
1. Highest-detail installed regional pack
2. Installed SD-card world overview
3. Embedded firmware world fallback
```

There must be **no automatic Internet fallback**.

If a regional pack is unavailable, OrcMaps should fall back to another eligible local source.

Future source selection should eventually work at visible-tile granularity so regional detail can coexist with the world overview near pack boundaries. That larger resolver improvement is separate from the embedded-world proof.

---

## 6. OrcSDR Use Case

The embedded map is particularly well suited to OrcSDR because OrcSDR needs geographic context more than street-navigation completeness.

Useful embedded-world features include:

- land / coastline
- oceans
- major lakes and rivers
- country boundaries
- major state/province boundaries
- selected major populated places once label rendering is available

It does **not** need to contain:

- local streets
- individual buildings
- full POI databases
- street addresses
- neighborhood-scale cartography

RF overlays should remain visually dominant over the base map.

Examples:

```text
ADS-B aircraft + track + range rings
        over
quiet geographic context
```

```text
LoRa node / bearing / range
        over
country/state/terrain context
```

---

## 7. Current Recommendation

If/when firmware embedding is implemented, start with the measured **z0-z4 Natural Earth PMTiles candidate (~0.83 MiB)**.

Reasons:

- It is already small enough to be practical as a flash asset.
- z5 costs materially more flash while current visual testing found little additional fallback value.
- richer world navigation is already better served by the z0-z7 SD pack.
- local/detail mapping belongs in regional SD packs.

Do not freeze this permanently until it has been physically tested on target hardware.

---

## 8. Implementation Requirements for Later

When this feature is resumed, the implementation should prove the following before becoming a product feature:

1. A generic flash-backed `ByteSource` can perform random access into the embedded PMTiles data.
2. The map is not copied wholesale into RAM.
3. The current OrcSDR application plus the map partition fit comfortably in the actual Tab5 flash layout.
4. Existing persistent partitions are not relocated unnecessarily.
5. A normal OrcSDR update preserves user settings/data.
6. OrcMaps can render the embedded map while OrcSDR's SDR, DSP, UI, Wi-Fi/C6, and storage workloads are active.
7. Missing or corrupt SD-card packs correctly fall back to the embedded map.
8. A corrupted embedded map fails safely rather than affecting SDR operation.
9. The embedded map can be independently identified and verified by version/hash.
10. The feature remains optional for OrcMaps consumers.

Potential release metadata:

```text
OrcSDR version:      <version>
Embedded map:        orcmaps-world-z4-<version>
Map source:          Natural Earth 5.1.2
Map SHA-256:         <hash>
Map profile:         orcmaps-overview-1
```

---

# CURRENT PRIORITY: SD-CARD PATH

The firmware-embedded world concept is intentionally deferred.

The current development priority is to continue proving the normal SD-card path across multiple embedded hardware tiers.

## 9. Next Test Bench Goal

Build a reusable OrcMaps SD-card benchmark/test bench that uses the same map artifacts, test viewports, instrumentation, and report format across devices.

Target hardware order:

```text
1. M5Stack Tab5 / ESP32-P4
2. LILYGO T-Display-S3
3. CYD 3.5-inch / ESP32-035 class board
```

Do not make an easier or device-specific map pack simply to obtain a passing result.

The purpose is to compare the same OrcMaps pipeline across hardware classes.

---

## 10. SD-Card Test Assets

The next bench should use at least:

```text
world-overview-z7.pmtiles
    complete worldwide overview
    measured size: ~9.29 MiB

existing Springfield / regional-detail PMTiles
    used for higher-zoom detailed testing
    existing proven Springfield reference: z14
```

Where practical, keep the same exact artifact hashes across all devices.

The world overview and regional detail pack should remain separate artifacts.

---

## 11. Standard Test Views

Use a consistent sequence where supported:

### World-overview tests

```text
Whole Earth
North America
Pacific Northwest
Oregon
Springfield approach
```

Test representative zooms through the selected world-overview range.

### Regional-detail tests

Use the existing Springfield reference location and retain the previous proven **z14** case as a comparison point.

Where useful, include z13 and z15 around the z14 baseline.

---

## 12. Required Benchmark Metrics

Each hardware report should capture, at minimum:

```text
Device / MCU
Display resolution
Flash size
PSRAM total
SD interface / clock
Map artifact name + SHA-256
Map file size
Viewport center
Zoom
Style
Visible tile count
Present tile count
Missing tile count
Bytes read from ByteSource

SD sequential read MB/s
Tile lookup time
Decompression time
MVT decode time
MVT → Feature translation time
Feature classification time
Render time
Total map frame time

Internal RAM before render
Internal RAM minimum during render
Largest internal block where available
PSRAM before render
PSRAM minimum during render
Largest PSRAM block where available
```

For hardware comparisons, clearly distinguish:

- cold load
- repeated/warm render
- power-to-map time
- map-frame time

Do not compare unlike measurements as if they are equivalent.

---

## 13. Tab5 Next

The Tab5 remains the primary high-end embedded reference.

Next Tab5 goals:

- run the new SD-card world-overview pack
- retain Springfield z14 regional benchmark for comparison with earlier results
- capture updated complete stage timings
- capture memory high/low watermarks
- confirm graphics output remains visually correct
- confirm the exact same OrcMaps core remains usable through the M5GFX adapter only at the final rendering boundary
- verify no runtime network dependency

Where possible, create a numbered device report on SD card so hardware results can be preserved and compared later.

---

## 14. LILYGO T-Display-S3 After Tab5

After the Tab5 bench is stable, run the same logical workload on the T-Display-S3.

The test should reuse:

- same world-overview artifact
- same regional-detail artifact
- same source pipeline
- same Feature/FeatureKind logic
- same style definitions
- same benchmark field names

The display viewport will naturally differ because the screen is smaller, but the map files must not be rebuilt specifically for the device.

The existing measured LilyGO result can remain a historical baseline, but the new bench should collect the complete common metric set so the comparison with Tab5 is fair.

---

## 15. CYD 3.5-inch After LILYGO

The CYD 3.5-inch board should be treated as a new hardware tier rather than assumed to match another CYD revision.

Before rendering maps, run a hardware inventory/probe and record:

- exact ESP32 chip model/revision
- CPU frequency
- flash size
- PSRAM detected / total / free
- display controller and resolution
- SD interface and usable speed
- available internal heap
- largest internal block
- display orientation / rotation

Then run the same SD-card benchmark artifacts.

Start with a static render. Do not add touch interaction until the static map path is proven.

If memory pressure causes failure, record the failure honestly. Do not silently simplify the map artifact to produce a pass.

That result may justify later streaming/direct-render optimizations in OrcMaps.

---

## 16. Test Bench Success Criteria

The next benchmark tranche succeeds when:

- the same world-overview PMTiles pack renders correctly on Tab5, T-Display-S3, and CYD 3.5-inch
- the same regional-detail pack can be tested at the established Springfield viewport where hardware permits
- every device produces comparable timing/memory/storage metrics
- no test requires Internet access
- map files remain graphics-adapter-independent
- device-specific code is limited to storage/display/input integration
- test reports are preserved for future regression comparisons

---

## 17. Explicitly Deferred Until After SD Benchmarks

Do not prioritize these yet:

- firmware-embedded world-map implementation
- flash-partition redesign
- automatic world → regional multi-pack composition
- user map annotations/tags
- map annotation persistence implementation
- cloud pack service
- automatic map downloading on-device
- full OSM planet benchmark
- second graphics framework integration solely for feature expansion

These ideas remain valid planned work, but the immediate goal is to finish the SD-card benchmark path first.

---

## 18. Governing Principle

OrcMaps should prove the simplest robust storage path before adding additional storage tiers.

For now:

```text
SD card
   ↓
PMTiles
   ↓
OrcMaps
   ↓
generic renderer
   ↓
device graphics adapter
```

Once that path is repeatedly proven across ESP32-P4, ESP32-S3, and classic ESP32 hardware, the embedded-world flash source can be added as another `ByteSource` without changing the map/rendering architecture.

The firmware idea is therefore preserved as a planned capability, not allowed to interrupt the current hardware-validation sequence.
