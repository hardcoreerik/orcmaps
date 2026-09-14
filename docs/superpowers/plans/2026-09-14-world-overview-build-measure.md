# Natural Earth World Overview Build and Measurement Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build, render, and measure local z6/z7/z8 Natural Earth overview candidates while keeping OrcMaps graphics-adapter independent.

**Architecture:** A small Planetiler YAML profile emits `orcmaps-overview-1` MVT into one z8 master; a standard-library Python orchestrator verifies fixed local inputs and invokes pinned host-only tools, then go-pmtiles derives z6/z7. OrcMaps adds one centralized profile predicate and profile-aware experimental classification, while existing generic host rendering supplies visual and timing evidence.

**Tech Stack:** C++17 OrcMaps core/tests, Python standard library, Planetiler 0.10.2, Java 21, go-pmtiles v1.28.2, PMTiles v3/MVT/gzip.

**Spec:** `docs/superpowers/specs/2026-09-14-world-overview-build-measure-design.md`

## Global Constraints

- Use only Natural Earth 5.1.2 assets already under ignored `data/local/world-overview/natural-earth/5.1.2/`.
- Do not use OpenMapTiles to build the world overview.
- Do not add M5GFX, M5Unified, LVGL, board, Tab5, display-controller, or OrcSDR dependencies to the pack/core path.
- Builders must not download; go-pmtiles provisioning is an explicit separate local action.
- Do not commit generated PMTiles, renders, tools, extracted sources, or temporary files.
- Do not publish, flash hardware, redesign the resolver, merge to `main`, or download the OSM planet.

---

### Task 1: Centralize profile compatibility and overview classification

**Files:**
- Modify: `include/orcmap/pack.hpp`
- Modify: `src/core/pack.cpp`
- Modify: `include/orcmap/experimental/mvt_classify.hpp`
- Modify: `src/tiles/mvt_classify_experimental.cpp`
- Test: `tests/host/test_pack.cpp`
- Test: `tests/host/test_feature.cpp`

**Interfaces:**
- Produces: `bool IsSupportedSchemaVersion(std::string_view schema_version)`.
- Produces: `bool AssignFeatureKindsForProfile(std::string_view schema_version, FeatureTile* tile)`.
- Preserves: existing `AssignFeatureKinds(FeatureTile*)` OpenMapTiles behavior.

- [ ] Write a host test asserting both named profiles validate and an unknown profile returns `kCompatibility`.
- [ ] Run the host test and verify it fails because `orcmaps-overview-1` is rejected.
- [ ] Add the two-value `IsSupportedSchemaVersion` predicate and route manifest validation through it.
- [ ] Run the host test and verify it passes.
- [ ] Write a host test building five literal overview features and asserting land/water/waterway/boundary/place map to the required `FeatureKind` values through `AssignFeatureKindsForProfile`.
- [ ] Run the host test and verify it fails because the profile-aware entry point is absent.
- [ ] Implement the minimal profile-aware classifier outside the decoder, delegating existing OpenMapTiles behavior and rejecting unknown profiles.
- [ ] Run host tests and commit the focused compatibility/classification change.

### Task 2: Add the profile and reproducible builder

**Files:**
- Create: `tools/pack-builder/orcmaps-overview-1.yml`
- Create: `tools/pack-builder/build_world_overview.py`
- Create: `tests/test_world_overview_builder.py`
- Modify: `tools/pack-builder/README.md`

**Interfaces:**
- Consumes: pinned `world_overview_sources.json`, local `SOURCE.json`, Planetiler JAR, Java executable, and caller-supplied go-pmtiles executable.
- Produces: `validate_inputs`, `build_commands`, `archive_metrics`, `pack_id`, `write_sidecars`, and a noninteractive CLI with `--dry-run`, `--force`, `--source-root`, `--output-dir`, `--planetiler-jar`, `--java`, and `--pmtiles-cli`.

- [ ] Write tests with temporary source records/files and the committed tiny PMTiles fixture that prove hash/component rejection, the exact 110m/50m/10m zoom mapping, five required output layers, deterministic world identity, overwrite refusal, local-only commands, archive distributions, and manifest/candidate metadata.
- [ ] Run the targeted Python tests and verify each new behavior fails for the intended missing implementation.
- [ ] Add the smallest declarative YAML profile, using argument-based local paths and no URLs.
- [ ] Implement the standard-library builder to validate inputs/tools, build z8 once, derive z6/z7, and write sidecars/evidence without networking.
- [ ] Run targeted tests until green, then all Python tests.
- [ ] Run Planetiler's profile verifier against the committed YAML using only controlled local inputs.
- [ ] Commit the focused profile/builder/test change.

### Task 3: Provision tools and build candidates locally

**Files:**
- Local only: `data/local/tools/go-pmtiles/v1.28.2/`
- Local only: `data/local/world-overview/build/`

**Interfaces:**
- Consumes: official upstream go-pmtiles v1.28.2 release asset and its upstream provenance/checksum evidence when available.
- Produces: version/hash evidence plus `world-overview-z8`, `world-overview-z7`, and `world-overview-z6` immutable triplets.

- [ ] Resolve the official Windows release asset and published checksum/provenance from upstream.
- [ ] Download it explicitly to ignored local storage, verify its SHA-256 and reported version, and record the release URL.
- [ ] Run the builder dry run and inspect every command for local-only inputs and no download flags.
- [ ] Run the z8 build once and record Planetiler/Java versions, exact command, duration, and peak memory when available.
- [ ] Derive z7 and z6 using the verified go-pmtiles executable.
- [ ] Verify all three archives open, match their manifest/checksum, retain `orcmaps-overview-1`, and contain only the five required layers.
- [ ] Confirm Git ignores all generated/tool files.

### Task 4: Measure and render through the generic host path

**Files:**
- Use unchanged: `tools/pack-inspect/main.cpp`
- Create: `docs/evidence/WORLD_OVERVIEW_HOST_MEASUREMENTS.md`

**Interfaces:**
- Consumes: each candidate and existing `FramebufferTarget`, `MapStyle`, PMTiles reader, decompressor, MVT decoder, translator, and classifier.
- Produces: archive distributions, representative pipeline timings, feature/RAM observations, and ten local PPM/PNG renders.

- [ ] Use `archive_metrics` for archive-wide distributions and characterize existing pack-inspect preview/sample output against each candidate.
- [ ] Measure tile counts plus stored/decompressed average, median, and maximum sizes for z6/z7/z8.
- [ ] Render world, North America, Pacific Northwest, Oregon, and Springfield approach with `standard-light` and `orcsdr-dark` through the generic framebuffer.
- [ ] Record lookup, decompression, decode, translate/classify, render, total-frame, representative feature, and practical RAM measurements.
- [ ] Record data-content quality separately from renderer visibility and recommend tiny/standard cutoffs plus the regional handoff zoom from evidence.
- [ ] Commit only the Markdown evidence and any tested generic inspection code; keep renders local.

### Task 5: Independence gate, truth updates, and final verification

**Files:**
- Create: `tools/check_world_overview_independence.py`
- Create: `tests/test_world_overview_independence.py`
- Modify: `STATUS.md`
- Modify: `ROADMAP.md`
- Modify: `PROJECT_TRUTH.md`
- Modify: `docs/GLOBAL_MAP_STRATEGY.md`
- Modify: `docs/PACK_MANIFEST_SCHEMA.md`
- Modify: `docs/DEPENDENCY_LEDGER.md`

**Interfaces:**
- Consumes: committed profile/builder/core path and generated evidence.
- Produces: an automated forbidden-dependency gate and factual project documentation.

- [ ] Write the independence test for a checker that copies the tracked project to a temporary tree, excludes `adapters/m5gfx`, configures/builds/runs host and consumer tests there, and dry-runs the overview builder against caller-supplied local inputs.
- [ ] Run it and verify it fails because `check_world_overview_independence.py` is absent.
- [ ] Implement the checker with temporary-directory cleanup and explicit local paths, then run it green.
- [ ] Update project truth documents with measured artifacts, profile support, political-boundary caveat, local source selection note, and explicit unimplemented items.
- [ ] Run host C++ tests, all Python tests, consumer build/test, Planetiler profile verification, documentation truth, provenance truth, and `git diff --check`.
- [ ] Verify `git ls-files data/local` is empty and no PMTiles/render/tool/temp file is staged.
- [ ] Review the complete branch diff, commit the final evidence/docs/gate, and stop without merging.
