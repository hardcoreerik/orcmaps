# OrcMaps Map Provisioning and Generation Design

## Goal

Close the gaps between "OrcMaps can render a pack someone already built by
hand" and "a user can get maps onto a device, and change them if they want
to."

## Two user populations, deliberately separated

This is the decision that shapes everything else.

**Population A — deploy once, then done.** The majority. They want working
maps on the SD card and never want to think about map data again. They must
**not** need Java, Planetiler, go-pmtiles, Python, a CLI, bbox coordinates,
or an understanding of PMTiles. For them the product is a **provisioner**:
pick a device, pick coverage, write the card, verify it, done.

**Population B — want the ability to change.** A minority. Custom regions,
custom zoom ranges, their own OSM extracts, rebuilds when data ages. For
them the product is a **generator**, and a heavy external toolchain is an
acceptable cost — but it must be detected and reported honestly, never
assumed.

Conflating these two is the main risk. A tool that makes Population A
install a JDK has failed, and a tool that hides real build requirements
from Population B has lied.

## Non-goals

- No hosted build service, no network requirement at render time, no
  telemetry, no account, no license server.
- No Internet fallback for missing tiles, ever.
- No new map format, no new schema freeze (`openmaptiles-3.16` and
  `orcmaps-overview-1` both stay unfrozen).
- No GUI in the first tranche. The `.exe` is a CLI with a provisioning
  default; a graphical wrapper is a later, separate decision.
- No OrcSDR changes.

## Honest gap analysis

What exists today (post-merge):

| Piece | State |
|---|---|
| `tools/pack-builder/acquire_world_overview_sources.py` (194 lines) | works, Natural Earth only |
| `tools/pack-builder/build_world_overview.py` (229 lines) | works, overview only |
| `tools/pack-builder/build_regional_pack.py` (226 lines) | works, `--bbox`/GeoJSON from an existing archive |
| `tools/pack-builder/build_springfield_pack.ps1` | works, **PowerShell only** |
| `tools/pack-inspect/` (C++) | implemented, host-only |
| `tools/pack-verify/` | **empty directory** |

The gaps that actually block the two user stories:

1. **No single entry point.** Four builders across three languages
   (Python, PowerShell, a Java Planetiler profile) with different argument
   conventions. Nothing a user can be pointed at.
2. **No importable API.** All logic lives in `main()`-shaped scripts, so
   nothing can be reused, tested in isolation, or driven by the `.exe`.
3. **No verifier.** `tools/pack-verify/` is empty. Nothing validates a
   finished triplet end-to-end.
4. **Manifest emission and manifest validation are separate truths.**
   Python writes manifests; C++ `ValidatePackManifest()` judges them. They
   have already diverged: `build_world_overview.py` writes bounds as
   `85.0511288`, which `ValidBounds()` rejects (it requires
   `<= 85.05112878`). **Every overview manifest currently on disk fails the
   engine's own validator.** That class of bug must be made impossible, not
   just fixed once.
5. **The device cannot discover packs.** Runtime manifest JSON discovery is
   not implemented, so the Tab5 demo compiles manifests in. Until this
   exists, *no* amount of generation tooling produces a pack a user can
   simply copy and use — this is the true blocker for both populations.
6. **No on-device integrity check.** `output_sha256` is recorded but never
   verified on the device.
7. **Heavy toolchain is undeclared.** Java 21 + Planetiler 0.10.2 +
   go-pmtiles 1.28.2 are required for generation and silently assumed.
8. **No coverage vocabulary for humans.** Users must supply raw bounding
   boxes; there is no named-region concept.
9. **Generation-time provenance is unenforced.**
   `tools/check_data_provenance.py` validates *committed* manifests in this
   repository, not arbitrary user output.

## Layer 1 — One manifest truth (foundation)

A single authoritative description of the pack manifest, consumed by both
languages, so emission cannot drift from validation:

- `docs/PACK_MANIFEST_SCHEMA.md` stays the human contract.
- Add a machine-readable schema (`data/schema/pack-manifest-1.json`) that
  the Python emitter validates against before writing, and that the
  verifier uses.
- **Cross-language regression gate**: a host test feeds
  Python-emitted manifests to the real C++ `ValidatePackManifest()`. Fix the
  `85.0511288` rounding as part of this, using the engine's
  `kMercatorMaxLatDeg`; `llround(x * 1e7)` keeps `pack_id` identity stable.
- Repair the manifests already written under `data/local/world-overview/`.

Exit criteria: a manifest that any OrcMaps tool writes is accepted by the
engine, proven by a test rather than by inspection.

## Layer 2 — Python API

`tools/pack-builder/orcmaps_pack/` as an importable package, stdlib-only
where practical (matching the checkers' no-dependency discipline):

| Module | Responsibility |
|---|---|
| `regions.py` | named regions -> bounds/GeoJSON; the human coverage vocabulary |
| `sources.py` | acquire + verify pinned source bundles (Natural Earth today, OSM extracts next) |
| `toolchain.py` | locate/verify Java, Planetiler, go-pmtiles; pinned versions and hashes |
| `build.py` | overview and regional builds, one code path per pack class |
| `manifest.py` | emit + schema-validate manifests, compute identity |
| `verify.py` | validate a triplet: hash, schema, identity, zoom/bounds, provenance ids |
| `provision.py` | write a prepared pack set to a target directory/card, then verify |

Existing scripts become thin wrappers so nothing currently working breaks.
`build_springfield_pack.ps1` is superseded by a cross-platform path.

## Layer 3 — Unified CLI

One command, subcommands shaped by population:

```
orcmaps provision      # Population A: default, no build toolchain
orcmaps verify         # check a card or directory
orcmaps regions        # list named coverage
orcmaps doctor         # report toolchain state honestly
orcmaps sources        # acquire/verify pinned source data   (advanced)
orcmaps build          # generate a custom pack              (advanced)
orcmaps inspect        # existing C++ pack-inspect, surfaced
```

`provision` and `verify` must work with **no** Java/Planetiler present.
`build` refuses to start with a clear report when `doctor` fails, rather
than failing midway through a multi-gigabyte job.

## Layer 4 — Windows executable

`orcmaps.exe`, PyInstaller one-file, default subcommand `provision`.

**What it does not bundle:** Planetiler (Apache-2.0, ~100 MB) and a JRE are
**not** embedded. Population A never needs them; Population B gets them via
`orcmaps sources`/`doctor`, which fetch the pinned Planetiler jar and verify
its SHA-256, and detect an installed JDK 21. Bundling a JRE is an explicit
open decision (size, plus its own licence terms) — see Open decisions.

**Provisioning flow for Population A:**

```
orcmaps.exe
  -> detect removable drives, ask which card
  -> choose coverage (world overview, optional region)
  -> copy/download verified packs + manifests + checksums to /orcmaps/
  -> re-verify every file's SHA-256 on the card
  -> print exactly what was written and what attribution applies
```

No partially written card may be reported as success; write to a temporary
name and rename only after hash verification, mirroring the atomic
activation pattern OrcSDR already proved.

## Layer 5 — Device-side runtime discovery (the real unblocker)

Generation is pointless to a user until the firmware reads the card. This
layer is therefore **not optional** and is sequenced before the `.exe` ships:

- A minimal, bounded JSON manifest reader in the engine
  (`src/core/pack_json.cpp`), host-tested against the committed manifests,
  with hard limits on size/nesting and no dynamic schema.
- Discovery: enumerate `/orcmaps/*.manifest.json`, validate, add to
  `PackCatalog`, resolve as today.
- Optional streamed on-device SHA-256 of the archive, reported through the
  benchmark schema (verification time is a measurement, not hidden cost).
- The Tab5 demo switches from compiled-in manifests to discovery, which
  also deletes the compiled-in duplication in `examples/m5stack-tab5`.

## Licensing, provenance, and signing

Staying consistent with existing project rules:

- **Engine/tool licence**: the CLI and `.exe` are OrcMaps code, so
  AGPL-3.0-only applies; distributing the binary carries source-availability
  obligations. `LICENSING.md` gains a short note.
- **Bundled/fetched third parties** go in `docs/DEPENDENCY_LEDGER.md`
  before use: PyInstaller (runtime hook licensing), Planetiler
  (Apache-2.0), go-pmtiles (BSD-3-Clause), and any JRE if that decision
  changes.
- **Map data provenance travels with the pack.** A provisioned card must
  carry the manifest, including `required_attribution`; `provision` prints
  it and `verify` fails a pack whose `provenance_ids` are unknown to
  `data/sources/`. Natural Earth is clean-class; the Springfield/OSM pack
  is ODbL and keeps its attribution and share-alike obligations.
- **User-generated packs are the user's own** — the tool must not imply
  OrcMaps ownership of output, and must not silently upload anything.
- **Windows code signing** is an open question: an unsigned `.exe` trips
  SmartScreen/Defender, which is a real adoption gap for Population A.

## Phasing

| Phase | Content | Exit criteria |
|---|---|---|
| P1 | Layer 1 manifest truth + bounds fix + `pack-verify` | Python-emitted manifest passes C++ validator in a test; verifier catches a corrupted triplet |
| P2 | Layer 2 API + Layer 3 CLI (`verify`, `regions`, `doctor`, `inspect`) | existing builds reproduce byte-identically through the API |
| P3 | Layer 5 device discovery + on-device hash verify | Tab5 boots from a card-discovered pack with no compiled-in manifest |
| P4 | `orcmaps build` unified generation | a named region builds end-to-end with a valid triplet on a clean machine |
| P5 | `orcmaps.exe` provisioning | a non-technical user writes a working card with no Java/Python installed |

P3 gates P5: the `.exe` must not ship before a provisioned card actually
works unmodified.

## Open decisions

1. **Prebuilt pack hosting.** Provisioning for Population A needs
   downloadable packs (GitHub Releases vs object storage). Data hosting
   stays separate from code hosting per `PROJECT_TRUTH.md`. Until this
   exists, `provision` can only copy from a local directory.
2. **Which coverage ships as the default deployment?** Recommend the
   z0-7 world overview (9,737,500 B) plus optional regional packs.
3. **JRE bundling** for Population B: require installed JDK 21 (smallest,
   most honest) vs fetch a pinned Temurin (largest convenience, extra
   licence terms).
4. **Code signing certificate** available for `orcmaps.exe`?
5. **GUI later?** CLI-with-provisioning-default first; a small GUI only if
   Population A still struggles.

## Documentation obligations

Nothing here is complete until the docs move with it:

- new user-facing `docs/MAP_GENERATION.md` (both populations, plainly
  written, exact SD layout);
- `docs/PACK_MANIFEST_SCHEMA.md` updated for the machine-readable schema;
- `docs/DEPENDENCY_LEDGER.md` for PyInstaller/Planetiler/go-pmtiles/JRE;
- `docs/DATA_AND_LICENSING.md` for user-generated pack provenance;
- `PROJECT_TRUTH.md` for the two-population split and the AGPL note;
- `ARCHITECTURE.md` for the tool layers and device discovery;
- `ROADMAP.md`/`STATUS.md` per phase, with both Truth checkers passing.
