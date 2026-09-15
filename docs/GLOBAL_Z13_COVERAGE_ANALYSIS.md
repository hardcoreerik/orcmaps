# Global navigation to z13: size, file layout, and what blocks it

Scope: what it would take for the Tab5 demo to let a user navigate to **any
place on Earth down to z13**, and what that costs in bytes and files.

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

## Size: what we can and cannot claim

**We cannot state a reliable figure from our own data, and should not
pretend otherwise.** Our only OSM sample is the Springfield extract:
3,507,636 B for 50.4 km^2 at z0-15, i.e. ~69.6 kB/km^2. Extrapolating that
across 148.9 M km^2 of land gives absurd results (70-140 GB at z0-13, above
published whole-planet figures at higher zoom), because a built-up US city is
nowhere near the global mean density. One dense urban sample cannot size the
planet.

What can be said with the anchors we do have:

- `data/local/oregon-latest.osm.pbf` is 253,592,262 B (242 MiB) for one US
  state. A planet PBF is roughly two orders of magnitude larger.
- Published planet vector basemaps are on the order of 100 GB at z0-15.
  Each dropped zoom divides the total by roughly 3-4, so z0-13 lands in the
  **single-digit to low-tens of GB**. This is a recalled public figure, not
  something measured here, and no network was used to check it.

**The honest way to get a real number is to measure it**, and the toolchain
to do so is already on disk (`planetiler.jar`, go-pmtiles, the Oregon PBF).
Building Oregon at z0-13 and dividing by its share of global land gives a
defensible extrapolation for a few minutes of compute. Until that is run,
treat any global figure in this document as an order of magnitude.

## File layout that follows from the limits

Assuming z0-13 global lands anywhere in the 6-20 GB range:

- it fits the 29 GB card at the low end and is tight at the high end;
- it **cannot** be one file — at 2 GiB per archive it needs roughly
  **4-11 archives**, split geographically (continent or sub-continent);
- each archive needs its own manifest, and coverage must tile the globe
  without gaps, because `ResolvePack()` requires *full* coverage of the
  visible bounds and will otherwise fall through to partial rendering.

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

Generating planet z0-13 is a heavyweight job: a planet PBF download in the
tens of GB, Planetiler needing substantial RAM and scratch disk, and hours
of runtime. That is Population B territory in the provisioning design, not
something to put in front of Population A. Source acquisition over the
network is fine and expected; the prohibition is on runtime network use and
on scraping tile services.

## Recommendation

Staged, so each step is verifiable:

1. **Now (done):** world view at z2 with wrapping, plus the existing
   regional pack. Navigable globe at z2-z7, one city at z13-z15.
2. **Measure before scaling:** build Oregon z0-13 with the local toolchain
   to get a real bytes-per-area figure, and benchmark lookup on the largest
   archive produced. Both are cheap and remove the two biggest unknowns.
3. **Fix the 64-bit offset bug** in both byte sources before any archive
   approaches 2 GiB.
4. **Implement runtime manifest discovery** (P3). Without it no pack set
   larger than two entries can be used at all.
5. **Then** decide global z13 versus a curated set of regional z13 packs.
   A card holding the z0-7 world plus a handful of chosen z0-13 regions
   delivers most of the demo value for a few hundred MB and needs no planet
   build.

Step 5's curated option is worth taking seriously: "navigate anywhere at
z13" and "navigate anywhere, with z13 detail where the user actually cares"
differ by an order of magnitude in cost and are hard to tell apart in a demo.
