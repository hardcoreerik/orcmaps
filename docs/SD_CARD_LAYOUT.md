# SD card layout for OrcMaps packs

The contract a user has to satisfy, in full. It is deliberately flat and
boring: copy files into one directory, boot the device.

## Directory contract

```
/orcmaps/
    <name>.manifest.json      pack metadata  (REQUIRED)
    <name>.pmtiles            the archive it describes (REQUIRED)
    <name>.sha256             optional checksum sidecar, not read at boot
```

Rules, all enforced by `orcmap::DiscoverPacks()`:

- **Names pair by stem.** `oregon.manifest.json` describes `oregon.pmtiles`.
  The archive is located by replacing the suffix, so a manifest never
  contains a path — a file on a card cannot point the engine at something
  else.
- **One directory, no recursion.** `/orcmaps/` is listed once.
  Subdirectories are ignored, not walked.
- **A manifest without its archive is reported and skipped**, and so is an
  archive without a manifest. An archive alone is invisible: metadata is
  what makes a pack usable.
- **Anything else in the directory is ignored**, including the numbered
  `orcmaps-benchmark-NNNN.jsonl` reports the demo writes there.
- The device mounts the card at `/sd`, so the firmware scans
  `/sd/orcmaps`. The path belongs to the board, not to OrcMaps core.

## Verify before you boot

`tools/pack-verify` runs the **same** `DiscoverPacks()` the firmware runs,
so a card that passes here behaves the same on the device:

```
cmake -S tools/pack-verify -B build-pack-verify
cmake --build build-pack-verify --config Release
build-pack-verify/Release/orcmap_pack_verify G:/orcmaps
```

It prints what would install, what would be refused and why, and warns when
a manifest claims coverage or zooms beyond its archive's own PMTiles header.

## Why a pack gets refused

Discovery never guesses. Every refusal is reported on serial as a
`pack_rejected` JSONL record and in the `DiscoveryReport`:

| Reason | Meaning |
|---|---|
| `unreadable` | the manifest could not be read, or exceeds 64 KiB |
| `bad-json` | not a well-formed manifest document (see `PackJsonError`) |
| `invalid-manifest` | well-formed but `ValidatePackManifest()` refused it — including an **unsupported schema profile**, which is rejected rather than guessed |
| `archive-missing` | the manifest is fine, its `.pmtiles` is absent |
| `archive-size-mismatch` | archive present but not the size the manifest declares |
| `duplicate` | another manifest already provided this pack identity |

One bad file never hides the packs beside it: the scan continues and
installs everything else.

## The three-tier example card

This is the layout currently staged on the Tab5's card, and what produces
the world → regional → local demo:

| File | Bytes | Region | Zooms | Priority | Role |
|---|---:|---|---|---:|---|
| `world-overview.pmtiles` | 9,737,500 | world | z0–7 | 0 | global basemap |
| `oregon.pmtiles` | 84,615,534 | oregon | z1–13 | 10 | regional detail |
| `springfield.pmtiles` | 3,507,636 | springfield-97477 | z0–15 | 20 | deepest local detail |

Total 98.3 MB of 30.3 GB free.

**Priority orders overlap, it does not select coverage.** `ResolvePack()`
first requires a pack to *fully contain* the visible bounds at the current
zoom; among those that qualify, the highest priority wins. So:

- a whole-world view resolves to the overview (the only pack that contains
  it);
- inside Oregon at z8–13, Oregon wins over the overview, which has no tiles
  that deep;
- inside Springfield at z14–15, only Springfield reaches that far;
- inside Springfield at a zoom both store, Springfield's priority 20 beats
  Oregon's 10 — deeper local data wins where it exists.

Priority is not part of `pack_id`, so a provisioner may set it per card
without changing pack identity.

## Zoom range in the archive is not display policy

A pack containing z1 does not mean a device shows z1. The lowest usable view
zoom is a property of the **display**: the world must be at least as tall as
the map area, because Mercator latitude cannot wrap. On the Tab5's
1280x600 map area the z1 world is 512 px and would leave 44 px empty bands,
so the interactive UI starts and stops at z2 (`WorldViewZoom`). On a 170 px
tall display the same pack is usable at z0. See `docs/MAP_CONTROLS.md`.

## Attribution travels with the pack

Credits come from the manifest, never from firmware. A pack built with the
OpenMapTiles schema must carry **both** the OpenMapTiles and the
OpenStreetMap credit; the Natural Earth overview requires none. `pack-verify`
prints exactly what each pack declares, which is what the device displays.
See `docs/DATA_AND_LICENSING.md`.
