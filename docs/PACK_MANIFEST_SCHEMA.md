# Pack manifest schema

Status: **schema/direction established, not yet implemented.** No pack
builder exists yet (`ROADMAP.md` Phase 3), so nothing produces these
manifests today. This document exists so the shape is decided before the
builder is written, rather than invented ad hoc partway through — see
`docs/DATA_AND_LICENSING.md` "Required pack provenance metadata" for the
policy reasons behind each field.

## Why a manifest, separate from the pack file

An OrcMaps map pack is a `.pmtiles` archive (`docs/PACK_FORMAT.md`) plus a
sidecar manifest and checksum:

```
oregon-clean.pmtiles
oregon-clean.manifest.json
oregon-clean.sha256
```

The manifest is not baked into the archive's own metadata section for one
important reason: PMTiles' internal `metadata` blob is opaque
application-defined JSON with no OrcMaps-specific validation, while the
sidecar file is what `tools/check_data_provenance.py` (once extended — see
below) and any future pack-install flow can validate deterministically
*before* opening the (potentially large) archive at all.

## Fields

```json
{
  "pack_id": "oregon-clean",
  "pack_version": "2026.09",
  "display_name": "Oregon (Clean)",

  "region": "Oregon, USA",
  "bounds": { "min_lon": -124.6, "min_lat": 41.9, "max_lon": -116.4, "max_lat": 46.3 },
  "min_zoom": 0,
  "max_zoom": 14,

  "map_schema_version": "orcmaps-vector-schema-v1",
  "pmtiles_version": 3,
  "mvt_schema_version": "orcmaps-vector-schema-v1",

  "builder": "orcmaps-pack-builder",
  "builder_version": "0.1.0",
  "builder_git_commit": "0000000000000000000000000000000000000000",
  "build_date": "2026-09-13",

  "sources": [
    { "provenance_id": "natural-earth", "acquired": "2026-09-01", "source_version": "5.1.2" },
    { "provenance_id": "us-census-tiger-line", "acquired": "2026-09-01", "source_version": "2024" }
  ],

  "pack_class": "clean",
  "required_attribution": [],

  "input_hashes": { "natural-earth": "sha256:...", "us-census-tiger-line": "sha256:..." },
  "output_sha256": "sha256:..."
}
```

- `sources[].provenance_id` **must** reference a real `id` in
  `data/sources/*.json` (`docs/DATA_PROVENANCE_REGISTRY.md`) — a manifest
  pointing at an unregistered or nonexistent source is exactly the kind of
  drift `tools/check_data_provenance.py` is meant to catch once manifests
  exist to check (see "Future checker extension" below).
- `pack_class` (`"clean"` / `"permissive"` / `"open"`) must agree with
  every referenced source's registry record: a `"clean"` pack can only
  reference sources whose `policy.clean_pack_allowed` is `true`.
- `required_attribution` is the exact list of attribution strings the
  runtime should surface via `orcmap::AttributionInfo`
  (`include/orcmap/attribution.hpp`) — derived from the referenced
  sources' `rights.attribution_required` at build time, not decided fresh
  by the application at runtime.
- `output_sha256` is the manifest's own promise about the paired
  `.pmtiles` file's content hash — verified against the actual
  `.sha256` sidecar and the file itself before a pack is trusted.

## Compatibility metadata

`map_schema_version` / `mvt_schema_version` / `pmtiles_version` let OrcMaps
determine whether an installed pack is readable by the running engine
version before attempting to parse it — this is what "prefer
backwards-compatible formats" (`ROADMAP.md`) is checked against in
practice, once there's more than one schema version in the wild.

## Future checker extension

Once real pack manifests exist, `tools/check_data_provenance.py` gains a
new check (not yet written, since there's nothing to check): every
`sources[].provenance_id` in every `*.manifest.json` must resolve to a
registry record, and that record's `policy` flags must actually permit the
manifest's declared `pack_class`. This is listed here rather than
implemented now specifically so the rule doesn't get invented differently
by whoever writes the pack builder later — see `PROJECT_TRUTH.md`
"Document authority."
