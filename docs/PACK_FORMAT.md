# Map pack format

OrcMaps map packs are standard **PMTiles v3** archives (`.pmtiles`), per the
decision in [`FORMAT_DECISION.md`](FORMAT_DECISION.md). OrcMaps does not
define its own container format — this document covers only what's specific
to how OrcMaps produces and consumes packs, not a restatement of the PMTiles
spec (`github.com/protomaps/PMTiles`, `spec/v3/spec.md`), which is the
normative reference for the byte format itself.

## What's implemented today

`orcmap::PmTilesReader` (`include/orcmap/pmtiles.hpp`,
`src/tiles/pmtiles_reader.cpp`) reads the container: header, root/leaf
directories (gzip or uncompressed), and tile-byte lookup by z/x/y via the
spec's Hilbert tile-ID addressing. It does **not** yet decode tile
*contents* (MVT vector geometry) — see `STATUS.md` for exact state.
`GetTile()` returns the tile's bytes as stored (still compressed per
`Header().tile_compression` if applicable); decompressing and interpreting
those bytes is the render layer's job, not the archive reader's.

## Metadata

PMTiles' `metadata` section holds a JSON blob (application-defined content,
per spec). OrcMaps packs must include, at minimum, the provenance fields
required by [`DATA_AND_LICENSING.md`](DATA_AND_LICENSING.md): `source`,
`source_url`, `source_date`, `license`, `attribution`, `generator`,
`generator_version`, `data_version`. These live in the metadata JSON, not
as new binary header fields — PMTiles' own header already carries bounds/
zoom/compression/tile-type, so OrcMaps doesn't duplicate those.

## Tile content schema — not yet decided

Whether tiles carry general-purpose OpenMapTiles/Shortbread-schema MVT or a
narrower OrcMaps-specific feature set is explicitly deferred — see
"Deferred decision" in [`FORMAT_DECISION.md`](FORMAT_DECISION.md). This
document will record the chosen schema (layer names, geometry types,
attribute keys actually used for road class/water/label text/etc.) once
the Lane County vertical slice produces a real pack and that decision is
made from measurement rather than guesswork.

## Multi-file / chunked archives

Per the yuiseki Cardputer precedent (see `FORMAT_DECISION.md`), a
whole-planet archive may need splitting into e.g. 2 GiB chunks purely to
work around FAT32's 4 GiB single-file limit — that is a filesystem
constraint, not a PMTiles format feature, and any such chunking scheme
belongs in the pack **installer** (reassembling/addressing chunks), not
this document, until it's actually implemented. Not yet designed.

## Pack discovery on device

Not yet implemented. Per `docs/ORCMAP1_AUDIT.md` §6, this will be
directory-scan based (enumerate installed `.pmtiles` files under a known
maps directory) rather than the fixed 16-slot table OrcSDR's current
`catalog_sync` uses — tracked in `ROADMAP.md`.
