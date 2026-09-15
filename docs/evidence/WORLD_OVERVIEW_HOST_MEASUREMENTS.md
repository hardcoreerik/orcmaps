# Natural Earth world-overview host measurements

Measured 2026-09-14 on Windows with the generic OrcMaps host framebuffer.
These are host measurements, not ESP32 performance claims. Generated packs
and renders remain under ignored `data/local/world-overview/build/`.

## Reproducible inputs and tools

- Natural Earth 5.1.2: 21 pinned archives, all SHA-256 checks passed.
- Planetiler 0.10.2, commit `0e5588c4a6e8c29a270a33afe8df62027d889604`.
- Java/Javac 21.0.12.1.
- go-pmtiles 1.28.2, commit `5898b1719526e4dec27615387e663786ec9f3fcd`.
- Official Windows x86-64 go-pmtiles ZIP SHA-256:
  `6276bc64c4499c9af12dc90accc84d9560061c4be4886fa1e738316258ef284d`.

The complete z8 build plus z7/z6 derivation took 13.6 seconds wall time;
Planetiler reported 9 seconds for its part. No builder command contained a
URL or enabled downloads.

## Candidate archives

| Candidate | Bytes | MiB | SHA-256 | Addressed tiles | Entries | Contents |
|---|---:|---:|---|---:|---:|---:|
| z0-z6 | 4,833,728 | 4.61 | `ae653cd1f422681a0fe4a401b1e7a914208983ff4822304aaad03dffd1b22241` | 5,461 | 3,054 | 2,411 |
| z0-z7 | 9,737,500 | 9.29 | `a6942c11782eb843235bbfdf78de89de0c6fea25c9a5a37abb67aab5cca4028c` | 21,845 | 9,140 | 7,371 |
| z0-z8 | 17,165,758 | 16.37 | `fbebdddd333221fd862c6c71dc4b40d434b07fefee6838a53ea7a210e0f8cb66` | 87,381 | 27,323 | 22,001 |

All three are clustered PMTiles v3 archives containing gzip-compressed MVT,
world bounds, local attribution, and `orcmaps_schema=orcmaps-overview-1`.

## Archive-wide tile payloads

Statistics count addressed tiles. Small all-water/all-land tiles account for
the 74-75 byte median.

| Candidate | Stored mean / median / p95 / max | Decompressed mean / median / p95 / max |
|---|---|---|
| z6 | 926 / 75 / 4,409 / 26,351 B | 1,103 / 55 / 5,502 / 38,817 B |
| z7 | 494 / 75 / 2,341 / 26,351 B | 557 / 55 / 2,728 / 38,817 B |
| z8 | 252 / 75 / 1,036 / 26,351 B | 262 / 55 / 1,144 / 38,817 B |

The largest tile is the z0 world tile. OrcMaps decoded and classified all 721
features in it. Its translated `FeatureTile` occupied approximately 312,320
bytes on this 64-bit host. The sampled Oregon z6 tile had 30 features and an
approximately 14,766-byte translated footprint; z7 had 10 / 4,309 bytes; z8
had 6 / 2,253 bytes. These estimates are not embedded heap measurements.

## Generic renderer timings

Seven measured runs after one warm-up, 320x170 `standard-light`, centered near
Springfield at each candidate's maximum zoom:

| Candidate | Frame | Lookup | Inflate | Decode | Translate | Classify | Render |
|---|---:|---:|---:|---:|---:|---:|---:|
| z6 | 1.854 ms | 0.016 ms | 0.076 ms | 0.188 ms | 0.032 ms | 0.001 ms | 0.248 ms |
| z7 | 1.493 ms | 0.255 ms | 0.014 ms | 0.029 ms | 0.005 ms | <0.001 ms | 0.118 ms |
| z8 | 1.895 ms | 0.721 ms | 0.021 ms | 0.040 ms | 0.005 ms | <0.001 ms | 0.072 ms |

Frame time includes framebuffer output. These tiny host times primarily prove
the generic pipeline and should not be extrapolated to SD-card or ESP32 timing.

## Visual findings and recommendation

Ten local renders cover world, North America, Pacific Northwest, Oregon, and
the Springfield approach in `standard-light` and `orcsdr-dark`. World,
continent, and state views are useful. At z6 the Springfield-containing tile
already includes named places and major rivers. z7 localizes those features;
z8 adds little useful local detail because Natural Earth has no street data.
The renderer currently does not draw text, even though place names are present.

Recommended, but not yet frozen product policy:

- **tiny:** z0-z6 (4.61 MiB), the best size/usefulness point.
- **standard:** z0-z7 (9.29 MiB), better approach-scale continuity.
- **do not make z8 the default:** it costs another 7.08 MiB without local roads.
- Prefer a local regional pack at z8 and above; a missing regional tile remains
  unavailable rather than triggering a network fallback.

Natural Earth boundary geometry is reproduced as supplied; this build does not
independently assert geopolitical positions.

## Firmware-embedded fallback experiment

The verified z8 master was not rebuilt. Official go-pmtiles v1.28.2 derived
z4 and z5 directly from that master. Each local candidate has the same
manifest/checksum triplet as z6/z7/z8 and records z8 as its source artifact.

| Candidate | Exact bytes | MiB | SHA-256 | Addressed tiles | Entries | Contents |
|---|---:|---:|---|---:|---:|---:|
| z0-z4 | 871,343 | 0.83 | `9aea08772bacf1f024d1da90cc52aa8fcf0b0e37405415dc7c91e75e56596f0f` | 341 | 312 | 265 |
| z0-z5 | 1,492,862 | 1.42 | `6f5c37f6cb505315e4c8a1d74efcf634fdb547b59128422ebf71c8ecf99addcc` | 1,365 | 1,007 | 787 |
| z0-z6 | 4,833,728 | 4.61 | `ae653cd1f422681a0fe4a401b1e7a914208983ff4822304aaad03dffd1b22241` | 5,461 | 3,054 | 2,411 |

Archive-wide decompressed tile distributions:

| Candidate | Mean | Median | p95 | Maximum | Representative Oregon tile |
|---|---:|---:|---:|---:|---:|
| z4 | 3,256 B | 1,076 B | 12,631 B | 38,817 B | 9,782 B / 95 features |
| z5 | 1,355 B | 249 B | 5,808 B | 38,817 B | 4,074 B / 54 features |
| z6 | 1,103 B | 55 B | 5,502 B | 38,817 B | 2,440 B / 30 features |

Seven measured runs after one warm-up, 320x170 `standard-light`, Oregon center,
at each candidate's maximum zoom:

| Candidate | Frame | Lookup | Inflate | Decode | Translate | Classify | Render |
|---|---:|---:|---:|---:|---:|---:|---:|
| z4 | 2.246 ms | 0.021 ms | 0.141 ms | 0.420 ms | 0.096 ms | 0.001 ms | 0.376 ms |
| z5 | 1.402 ms | 0.009 ms | 0.028 ms | 0.103 ms | 0.019 ms | <0.001 ms | 0.146 ms |
| z6 | 1.832 ms | 0.021 ms | 0.099 ms | 0.241 ms | 0.038 ms | 0.001 ms | 0.276 ms |

Twelve local generic-framebuffer renders cover world, North America, Pacific
Northwest, and Oregon for z4/z5/z6. z4 retains recognizable coastlines, major
water, country/state boundaries, and useful continental context. z5 costs
621,519 additional bytes (71% more) but does not materially improve a fallback
whose job is orientation rather than local navigation. z6 is useful as the
small downloadable overview but is unnecessarily large for firmware fallback.

**Recommendation: designate z0-z4, exactly 871,343 bytes, as the candidate
`OrcMaps Embedded World`.** Its data size is suitable for pursuing firmware
integration. Actual firmware-partition fit, boot access, and hardware render
remain separate unverified gates; this experiment did not modify firmware or
flash a device. The normal-user recommendation remains z7 with regional detail
beginning around z8.
