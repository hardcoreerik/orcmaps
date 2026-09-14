# OrcMaps map packs

## Runtime guarantee

OrcMaps is fully offline after provisioning. The network may be used by an
optional PC, CLI, web, or application downloader to obtain a pack, but it is
not part of the runtime architecture. Installed packs must remain usable with
Wi-Fi, cellular service, DNS, and the internet unavailable.

The only installed artifact is an immutable triplet:

```
<pack>.pmtiles
<pack>.manifest.json
<pack>.sha256
```

The manifest contains all runtime bounds, zoom, profile, compatibility,
provenance, attribution, priority, size, and hash metadata. Required
attribution text is local; optional links do not create a network dependency.

## Storage

The intended removable-storage layout is:

```
/orcmaps/packs/<pack>/
    <pack>.pmtiles
    <pack>.manifest.json
    <pack>.sha256
```

Manual SD copy and future Pack Manager tools install the same files. The
current Tab5 and LilyGO demos still use the transitional
`/orcmaps/springfield.pmtiles` path and do not yet ingest its sidecars.

## Selection

`PackCatalog` contains validated local manifests. A pack is eligible only when
its bounds contain the request and its zoom range includes the requested zoom.
`ResolvePack()` chooses the highest priority eligible pack; ties choose the
lexicographically greatest deterministic identity. Exactly one basemap is
selected. If none qualifies, detail is unavailable—there is no online tile
fallback.

## Current status

- **IMPLEMENTED:** portable manifest model and structural validation,
  deterministic identity, catalog, resolver, provenance-policy checks, and a
  real Springfield manifest/checksum triplet.
- **PARTIAL:** the Springfield demos open the archive directly.
- **PLANNED:** runtime JSON ingestion, directory discovery, streamed on-device
  SHA-256 verification, and atomic pack installation.

Product acceptance requires provisioning the packs, disabling all networking,
and completing the full map demonstration.
