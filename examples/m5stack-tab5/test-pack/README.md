# Springfield / 97477 public demo pack

Exact PMTiles archive used in the first hardware-verified Tab5 render
(firmware `1427597`, physical PASS) and reused by the LilyGO
T-Display-S3 demo. Copy this file; do not regenerate it just to run a
hardware demo.

| Field | Value |
|---|---|
| File | `springfield-97477.pmtiles` |
| Bytes | 3,507,636 |
| SHA-256 | `8bf23873915668f41d098b98df32b11a6d08ec6754a63d885fce2f29abe4adfd` |
| Manifest | `springfield-97477.manifest.json` |
| Checksum sidecar | `springfield-97477.sha256` |
| Viewport | 44.0500 N, 123.0220 W, z14, 1280×720, tile 256, `orcsdr-dark` |
| Expected tiles | 24 visible / 20 present / 4 missing |

Missing tiles are outside this bounded pack, not an engine error. A dark
strip on the west edge of the Tab5 image is pack coverage.

## SD card

1. Format microSD FAT32.
2. Create `/orcmaps/` on the card.
3. Current demos still expect this file at `/orcmaps/springfield.pmtiles`.
   The manifest and checksum are committed product-format evidence but the
   demo firmware does not ingest them yet.
4. Insert the card, then flash `examples/m5stack-tab5` or
   `examples/lilygo-tdisplay-s3` for the matching board.

See `ATTRIBUTION.md` (ODbL / © OpenStreetMap contributors).
