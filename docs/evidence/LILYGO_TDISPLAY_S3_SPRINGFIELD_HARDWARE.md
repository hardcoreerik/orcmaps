# LilyGO T-Display-S3 Springfield hardware evidence

**This is a real physical LilyGO T-Display-S3 Touch result, not a host
simulation.** Status: **hardware-verified static real-map render**. No
pan, zoom, labels, or touch interaction.

## Device and result

| | |
|---|---|
| Device | LilyGO T-Display-S3 Touch with SD Shield |
| MCU | ESP32-S3 revision 0.2, 8 MiB PSRAM |
| Display | ST7789, 320x170 landscape |
| ESP-IDF | 5.5.4 |
| Flash port | COM13 |
| Map | `/sd/orcmaps/springfield.pmtiles` |
| Result | **PASS** - Springfield map remained on screen |

The firmware flashed successfully and esptool verified every written
image hash. An initial physical run exposed a 170x320 orientation mismatch;
changing the display rotation from 1 to 0 corrected the landscape axes.
The user then confirmed the map rendered correctly and described
power-to-map as "faaaaast."

## Evidence boundary

| Gate | Result |
|---|---|
| ESP-IDF build | PASS |
| Flash and image verification | PASS |
| PSRAM initialization | PASS in pre-fix boot log |
| SD archive open and render | PASS by physical screen observation |
| 320x170 landscape orientation | PASS by physical screen observation |
| Exact stage and total timing | NOT CAPTURED |
| Touch input | NOT TESTED; outside this static demo |

The serial monitor was reattached without reset, but no second complete
boot log arrived. The qualitative speed observation is therefore not a
numeric benchmark and must not be compared directly with the Tab5 timing.

## SD report follow-up

A follow-up firmware was built and flashed through COM13 with verified image
hashes. After rendering, it overwrites
`/sd/orcmaps/orcmaps-report.txt` and displays `REPORT SAVED`. Retrieval of
that file remains the open timing-evidence gate.
