# Changelog

OrcMaps follows semantic versioning with pre-1.0 discipline (see
`PROJECT_TRUTH.md` "Public API and Versioning"): before 1.0 a public API may
change, but only in a new minor version, and every such change is listed here.
Consumers pin a release by its full commit SHA (`docs/ORCSDR_INTEGRATION.md`).

## 0.2.0 — 2026-09-24

First tagged release (`v0.2.0`). 0.1.0 was the version in
`idf_component.yml` from the start of the project; it was never tagged. This
entry lists what 0.2.0 adds over that baseline for a consumer.

### Added

- **`LocationPicker`** (`include/orcmap/location_picker.hpp`): a crosshair
  "choose your location" camera. The selection is the viewport centre; the
  opening zoom and the zoom-out floor are derived from the screen size and
  the data's coverage, so the view never shows empty margin.
  `LocationPickerZoomRange` exposes the same zoom choice.
- **`esp_idf::PartitionByteSource`**
  (`adapters/esp_idf/include/orcmap/esp_idf/partition_byte_source.hpp`):
  reads a PMTiles archive from a read-only flash data partition found by
  label, so a map can ship in the firmware image. The component now
  privately requires `esp_partition`.
- **Runtime pack discovery** (`include/orcmap/pack_discovery.hpp`):
  `DiscoverPacks` scans one directory for `<name>.manifest.json` +
  `<name>.pmtiles` pairs, validates each, and reports every rejection;
  `ResolvePack` picks the pack for a view. `esp_idf::PackFileSystem` is the
  ESP-IDF implementation.
- **Streaming decode** (`include/orcmap/mvt_stream.hpp`,
  `include/orcmap/compression.hpp`, `PmTilesReader::StreamTile`,
  `RenderFeatureAt`): a tile is inflated and parsed straight from the
  archive, one feature at a time, without ever holding the inflated tile.
  No single allocation exceeds roughly one feature's bytes. Also
  `PmTilesReader::GetTileInflated`, `TileExists`, `DecompressStreaming` and
  `InflatingByteStream`.
- **`tools/pack-builder/provision_pack.py`**: cuts an SD-ready pack for a
  dropped pin and radius from an existing pack, deriving all provenance from
  the source pack's manifest.
- **World overview builder** also derives the z0-z4 and z0-z5 packs;
  z0-z4 is the firmware-embedded world candidate.
- **`tools/embedded-world-check`**: opens a world pack through a
  partition-shaped byte source and frames it with `LocationPicker`.
- `docs/ORCSDR_INTEGRATION.md`: the interface OrcSDR can rely on.

### Changed

- `PmTilesReader(ByteSource*, int leaf_cache_slots = 2)`: the leaf-directory
  cache holds decompressed directory bytes (~21 KiB per slot) instead of
  parsed entries, and its slot count is a constructor argument (1 or 2).
- PMTiles directories are searched in their serialized form instead of being
  expanded into entry vectors.
- `DecompressPayload` keeps its ~8 KiB inflater state on the heap instead
  of the calling task's stack.

### Fixed

- World overview manifests used bounds slightly outside the engine's
  Mercator limit, so runtime discovery rejected them.
- OpenMapTiles-profile packs credited only OpenStreetMap, not the
  OpenMapTiles CC-BY grant, and the credit text was double-encoded.
- The host tests did not compile with GCC 13 (missing `<algorithm>`), and
  the consumer gate, `pack-inspect` and `pack-verify` did not link after
  streaming decode landed (missing `mvt_stream.cpp`).

### Known limitations

- Polygons are filled from their first ring only. On the Natural Earth
  world packs this leaves some tiles with unfilled land (visible as
  tile-shaped patches) and makes boundaries show only in those tiles.
- The streaming path does not verify the gzip CRC-32 of a tile.
- No tile cache and no overlay API yet.
- Feature classification is still `experimental::TryClassifyFeature`,
  and the renderer draws nothing without it.
