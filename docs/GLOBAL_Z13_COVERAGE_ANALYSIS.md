# Global navigation, z1 to z13: size, file layout, and what blocks it

Target range: **z1 through z13**, global.

Scope: what it would take for the Tab5 demo to let a user navigate to **any
place on Earth down to z13**, and what that costs in bytes and files.

## z1 is in the pack but is not the Tab5's floor

Choosing z1 as the bottom of the range costs nothing and is worth doing, but
it does not become the Tab5's zoom-out limit, and the reason is physical
rather than a policy choice:

| zoom | world size | fills the 1280x600 map area? |
|---|---|---|
| z0 | 256 x 256 px | no — 172 px empty band top and bottom |
| z1 | 512 x 512 px | no — 44 px empty band top and bottom |
| **z2** | 1024 x 1024 px | **yes** |

Width is not the problem: the map extends around itself, so any width is
covered. **Height cannot wrap** — Mercator Y is clamped, and repeating the
world vertically would be geographically false — so a world shorter than the
map area must show empty bands. z2 is therefore the lowest zoom that fills
this display, and `WorldViewZoom` derives exactly that.

Smaller displays reach further out: the LilyGO's ~170 px height is covered by
z0's 256 px world, so its floor is z0. The floor is a property of the display
and the installed coverage, never of the pack range alone.

Including z1 (and z0) in the pack is still correct — it costs **one tile**
versus starting at z1, it serves those smaller displays, and it keeps the
pack independent of any one device's chrome height. If the Tab5's bars were
ever reduced to leave a map area of 512 px or less, z1 would start filling
with no rebuild.

Tile counts, for scale: z0-13 is 89,478,485 tiles and z1-13 is 89,478,484 —
the range choice is not a size decision.

The world-view start (z2 on 1280x600, map extending around itself) is
**implemented**. Global z13 coverage is **not**, and this document separates
the parts that are hard limits from the parts that are merely large.

## Hard limits, verified in this repository

These are not estimates. They bound any answer.

| Limit | Value | Where it comes from |
|---|---|---|
| Max bytes addressable per archive | **2 GiB - 1** | `adapters/esp_idf/file_byte_source.cpp` seeks with `fseeko`/`off_t`; `off_t` is 32-bit signed on ESP-IDF |
| Max file size on the card | 4 GiB - 1 | FAT32 |
| exFAT (to lift the FAT32 cap) | **unavailable** | ESP-IDF 5.5.4 ships `FF_FS_EXFAT 0` in `components/fatfs/src/ffconf.h` with no Kconfig option; enabling it means patching vendored IDF source |
| Card capacity | 29,809 MiB total, 28,988 MiB free | the Tab5's own SD32G, measured |

So the **2 GiB per-archive limit binds first**, and it is ours, not the
card's. Any global pack set must be split into archives under 2 GiB
regardless of how big the card is.

The host adapter has the same defect in a different form:
`adapters/host/file_byte_source.cpp` casts the offset to `long`, which is
32-bit on Windows — so host-side verification of a >2 GiB pack would break
too. Both want `_fseeki64`/`fseeko64` and a 64-bit offset type before any
multi-gigabyte pack is attempted.

## Size: measured, not estimated

Rather than extrapolate from the Springfield extract — one dense urban tile,
which scales to figures above published whole-planet totals and is therefore
useless — a region-scale pack was actually built with the local toolchain:

```
java -Xmx6g -jar planetiler.jar --osm-path=oregon-latest.osm.pbf \
  --output=oregon-z1-13.pmtiles --minzoom=1 --maxzoom=13 \
  --render_maxzoom=13 --tile_compression=gzip --force
```

| Measured | Value |
|---|---:|
| Oregon, z1-13 | **84,615,534 B (80.7 MiB)** |
| Build time | 50 s |
| Intermediate feature store | 370 MB |
| Area | 254,800 km^2 |
| **Density** | **332 B/km^2** |
| Share of global land | 0.17% |

### The measured figure is authoritative; the global figure is not

**Authoritative:** Oregon z1-13 = 84,615,534 bytes. That is a build we ran
and can reproduce.

**Planning estimate only — order of magnitude:** everything below. It is a
single regional sample scaled by land area, which is not a sound basis for a
global total. Dense regions (western Europe, Japan, Korea, urban India) will
exceed Oregon's 332 B/km^2, possibly by a wide margin; sparse regions
(Sahara, Siberia, Antarctica, much of the ocean-adjacent coastline) will be
far below it. The distribution is also heavily skewed rather than uniform,
so an area-weighted mean from one sample can be wrong in either direction.

| Global mean density vs Oregon | Global z1-13 | Archives at 2 GiB |
|---|---:|---:|
| same as Oregon | ~49.5 GB | ~24 |
| half | ~24.7 GB | ~12 |
| a third | ~16.5 GB | ~8 |
| a fifth | ~9.9 GB | ~5 |
| a tenth | ~4.9 GB | ~3 |

Read this as "the answer is plausibly in the 5-50 GB range, so plan for
multiple archives and do not assume it fits one card", **not** as a bound.
49.5 GB is *not* a hard upper bound — it is simply what uniform
Oregon-density scaling produces, and real dense regions can exceed that
rate.

Only a measured multi-region or planet build would settle it, and no planet
build is required at this checkpoint.

## File layout that follows from the limits

- Global z1-13 **cannot be one file** at any plausible size: at 2 GiB per
  archive it needs several, split geographically (continent or
  sub-continent). This conclusion holds across the whole estimate range and
  does not depend on the estimate being accurate.
- Each archive needs its own manifest, and coverage must tile the globe
  without gaps, because `ResolvePack()` requires *full* coverage of the
  visible bounds and will otherwise fall through to partial rendering.
- Whether it fits one 32 GB card is genuinely unknown. If it does not, a
  larger card, or z12 globally with z13 only where wanted, both resolve it.

## Attribution finding: OpenMapTiles credit is missing

Planetiler's own output for this build states that tiles produced with the
OpenMapTiles profile are reusable under a CC-BY licence granted by the
OpenMapTiles team, and that **maps made with these vector tiles must display
a visible credit: "(c) OpenMapTiles (c) OpenStreetMap contributors"**.

The existing Springfield pack was built the same way, but its manifest
credits only `(c) OpenStreetMap contributors`, and `OpenMapTiles` appears
nowhere in `docs/DATA_AND_LICENSING.md` or `data/sources/`. The demo shows
whatever the manifest says, so the device is currently displaying an
incomplete credit.

This is **not** resolved here, and deliberately so: the authoritative text is
the OpenMapTiles licence itself, and project rules forbid recording a
provenance conclusion without checking the primary source. What is needed:

1. read the OpenMapTiles licence directly and record it as a source in
   `data/sources/` with its real terms;
2. add the required credit to the regional pack manifest and to any z1-13
   build, so attribution travels with the pack;
3. decide whether the overview pack is affected — it is Natural Earth via
   a custom `orcmaps-overview-1` profile, so probably not, but that needs
   checking rather than assuming.

Until then, treat any OSM/OpenMapTiles-derived pack as carrying an
unsatisfied attribution obligation.

## What blocks it beyond data

Data is not the only gap, and the firmware gaps are cheaper to fix:

1. **The demo can only hold two packs.** It compiles in exactly two
   manifests (`MakeWorldManifest`, `MakeRegionalManifest`). A continental
   pack set needs **runtime manifest discovery** — Layer 5 / P3 of
   `2026-09-14-map-provisioning-and-generation-design.md`, still
   unimplemented. This is the true blocker.
2. **No tile cache.** Every frame re-reads, re-inflates, re-decodes. The
   measured Tab5 frame is ~1.3 s for 18 tiles; panning a global map at z13
   would pay that repeatedly.
3. **Directory lookup cost at scale is unmeasured.** 276 ms of the 1,301 ms
   frame is already `lookup` on a 3.5 MB archive. PMTiles leaf-directory
   indirection on a ~2 GiB archive will be worse, and with no cache it is
   paid per frame. This is the largest unknown in the whole plan and should
   be measured on one large archive before committing to the pack layout.
4. **No labels.** At z13 a street map with no text is of limited use;
   `place` features render as dots and label kinds are skipped.

## Build cost for true global coverage

Generating planet z1-13 is a heavyweight job: a planet PBF download in the
tens of GB, Planetiler needing substantial RAM and scratch disk, and hours
of runtime. That is Population B territory in the provisioning design, not
something to put in front of Population A. Source acquisition over the
network is fine and expected; the prohibition is on runtime network use and
on scraping tile services.

## Recommendation

Staged, so each step is verifiable:

1. **Now (done):** world view at z2 with wrapping, plus the existing
   regional pack. Navigable globe at z2-z7, one city at z13-z15.
2. **Done:** Oregon z1-13 measured at 332 B/km^2 (above). Still to measure:
   PMTiles lookup cost on a large archive, the biggest remaining unknown.
   `oregon-z1-13.pmtiles` at 80.7 MiB is ~24x the Springfield archive and
   is a usable first data point for that.
3. **Settle the OpenMapTiles attribution obligation** before distributing
   any OSM-derived pack.
4. **Fix the 64-bit offset bug** in both byte sources before any archive
   approaches 2 GiB.
5. **Implement runtime manifest discovery** (P3). Without it no pack set
   larger than two entries can be used at all, so no multi-archive global
   layout is usable regardless of how the data is built.
6. **Then** decide global z1-13 versus a curated set of regional z13 packs.
   A card holding the z0-7 world plus a handful of chosen z1-13 regions
   delivers most of the demo value for a few hundred MB and needs no planet
   build.

The immediate next step that unlocks the most is **5**, not more data: the
demo physically cannot load more than two packs today.

Step 5's curated option is worth taking seriously: "navigate anywhere at
z13" and "navigate anywhere, with z13 detail where the user actually cares"
differ by an order of magnitude in cost and are hard to tell apart in a demo.
