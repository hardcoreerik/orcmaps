# Springfield / 97477 public demo pack

Exact PMTiles archive used in the first hardware-verified Tab5 render
(firmware `1427597`, physical PASS). Copy this file; do not regenerate it
just to run the demo.

| Field | Value |
|---|---|
| File | `springfield-97477.pmtiles` |
| Bytes | 3,507,636 |
| SHA-256 | `8bf23873915668f41d098b98df32b11a6d08ec6754a63d885fce2f29abe4adfd` |
| Viewport | 44.0500 N, 123.0220 W, z14, 1280×720, tile 256, `orcsdr-dark` |
| Expected tiles | 24 visible / 20 present / 4 missing |

Missing tiles are outside this bounded pack, not an engine error. A dark
strip on the west edge of the Tab5 image is pack coverage.

## SD card

1. Format microSD FAT32.
2. Create `/orcmaps/` on the card.
3. Copy this file to `/orcmaps/springfield.pmtiles` (that name, not
   `springfield-97477.pmtiles`).
4. Insert the card, flash `examples/m5stack-tab5`.

See `ATTRIBUTION.md` (ODbL / © OpenStreetMap contributors).
