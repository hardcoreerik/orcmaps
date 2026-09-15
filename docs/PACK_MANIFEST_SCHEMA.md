# Pack manifest schema

Status: **PARTIAL implementation.** `orcmap::PackManifest`, validation,
deterministic identity, `PackCatalog`, and local source resolution are
implemented and host-tested. The Springfield golden pack has a real manifest
and checksum sidecar. Runtime JSON discovery and streamed on-device archive
hashing are not implemented yet.

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
sidecar file is what `tools/check_data_provenance.py` and a pack-install flow
can validate deterministically
*before* opening the (potentially large) archive at all.

## Fields

```json
{
  "manifest_version": 1,
  "pack_id": "2026-09__openmaptiles-3.16__standard__us-or__b-1246000000_419000000_-1164000000_463000000__z0-14",
  "pack_version": "2026.09.1",
  "display_name": "Oregon",

  "region_id": "US-OR",
  "region_name": "Oregon, USA",
  "bounds": { "min_lon": -124.6, "min_lat": 41.9, "max_lon": -116.4, "max_lat": 46.3 },
  "min_zoom": 0,
  "max_zoom": 14,
  "content_profile": "standard",

  "pmtiles_version": 3,
  "schema_version": "openmaptiles-3.16",
  "source_snapshot": "2026-09",

  "builder": "orcmaps-pack-builder",
  "builder_version": "0.1.0",
  "builder_commit": "0000000000000000000000000000000000000000",
  "build_date": "2026-09-13",

  "sources": [
    { "provenance_id": "natural-earth", "acquired": "2026-09-01", "source_version": "5.1.2" },
    { "provenance_id": "us-census-tiger-line", "acquired": "2026-09-01", "source_version": "2024" }
  ],

  "pack_class": "clean",
  "required_attribution": [],
  "attribution_links": [],
  "priority": 10,
  "size_bytes": 123456,

  "input_hashes": { "natural-earth": "...64 lowercase hex..." },
  "output_sha256": "...64 lowercase hex..."
}
```

`pack_id` is derived from normalized immutable inputs in this order:
`source_snapshot`, `schema_version`, `content_profile`, `region_id`, bounds
normalized to signed degrees × 10,000,000, and zoom range. UI names,
requesting user, build machine, filename, board, and graphics
adapter do not participate. The current C++ form lowercases those identity
parts and joins them as
`<snapshot>__<schema>__<profile>__<region>__b<W>_<S>_<E>_<N>__z<min>-<max>`.
Identity parts are restricted to ASCII letters, digits, `-`, and `.`.

- `sources[].provenance_id` **must** reference a real `id` in
  `data/sources/*.json` (`docs/DATA_PROVENANCE_REGISTRY.md`) — a manifest
  pointing at an unregistered or nonexistent source is exactly the kind of
  drift `tools/check_data_provenance.py` catches for committed manifests.
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

`schema_version` / `pmtiles_version` let OrcMaps
determine whether an installed pack is readable by the running engine
version before attempting to parse it — this is what "prefer
backwards-compatible formats" (`ROADMAP.md`) is checked against in
practice. The currently supported measured payloads are Springfield's
`openmaptiles-3.16` and Natural Earth's narrow `orcmaps-overview-1`. This
records compatibility; it does not freeze either as OrcMaps' permanent general
schema.

## Local-only source selection

`PackCatalog` rejects invalid or duplicate identities and entries without a
resolved local archive path. The path is installed state, not part of the
immutable manifest or pack identity. `ResolvePack()` first
requires geographic and zoom coverage, then chooses the highest `priority`;
ties choose the lexicographically greatest deterministic identity. It returns
one pack or `nullptr`. It never renders overlapping basemaps and never fetches
a missing tile from a network.

## Offline boundary

Every field needed for bounds, zoom, schema compatibility, provenance,
attribution, priority, size, and integrity is local. A remote catalog may
later help users obtain the same immutable triplet, but it is not authoritative
for an installed pack and is never required to interpret or render it.
