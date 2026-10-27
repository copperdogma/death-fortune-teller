# Story 020 — Smaller Logo and Print Timing

**Status:** Done — human verified centered logo, signature and timing

## Scope and acceptance

Cam confirmed the existing print wait matches actual powered-printer activity. Reduce the skull logo to half its width and height. After the first hardware trial, Cam accepted its size, requested centering and the signature "-- Death, the Fortune-Teller", and restored printing to the fortune preamble. Preserve one print per Start, complete speech before goodbye, cancellation, and existing receiver pacing. No commit without an explicit request.

- [x] Embed a 192x177 version of the original 384x353 logo; preserve the original BMP.
- [x] Queue printing 250ms into the fortune preamble (restored after the initial Welcome experiment).
- [x] Verify one print, cancellation, missing clips, early printer completion, packet framing and payload checksum in host tests.
- [x] Build and install application-only firmware, preserving Matter fabric and card.
- [x] Human confirms the smaller logo is good and printing is fast enough.
- [x] Human confirms centered logo, new signature, complete fortune and final timing.

## Work log

### 2026-10-26 — Implementation and host validation

- Existing logo takes 353*(56*10/9600+0.050) = 38.24 seconds before text. Half-size output takes 177*(32*10/9600+0.050) = 14.75 seconds before text, excluding scheduler overhead. Keep the tested 50ms receiver gap and 9600 baud; this change does not attempt to fix historical receiver corruption by increasing throughput.
- Added dependency-free `native_scripts/generate_printer_logo.py`, which checks the source BMP format/palette and emits the flash header. Uses 2x2 area reduction with black at >=50% coverage; odd final source row pads white. Output pixel FNV1a a45dd837. Source BMP and SD files unchanged.
- Generate the fortune at accepted Start. Queue once 250ms after Welcome playback begins; if Welcome is absent, print immediately. A short Welcome falls back to queuing at preamble entry. Preparing/failing Welcome does not start a print. No second print at preamble entry/completion.
- Existing full-preamble and printer-idle gates still precede goodbye; a print completed during speech does not cut speech short. Stop cancels the earlier active print through the existing cancellation path.
- Death sanitizer suites and all general host suites passed. Native ESP-IDF5.5.4 build passed (app size 0x20dc50, 46% partition free). Physical print and concurrent audio/power behavior remain unverified for this revision.
- Reuses the established single-row GS v0 framing and pacing decisions in `docs/research/2026-10-25-printer-logo.md` and `docs/research/2026-10-25-printer-receiver-pacing.md`; no new protocol assumption or flow-control experiment.

### 2026-10-26 — Installed and boot checked

- Application-only write at generated offset0x30000 succeeded; esptool verified the flash data hash. Binary SHA256 `932e7bf89c3abe7c6c061ceed2933e90004cf115986d1588d6fa51d03b83791c`. Bootloader, partition table and NVS were not flashed. Build retains native SDMMC, SPI OFF and existing register diagnostics.
- Boot loaded all9 audio assets and startup PCM matched d6d8b5d5. Startup PCM delivered; saved speaker reconnected with stream1. Printer UART ready1/busy0, show Waiting, interruptions0. No print was triggered during deployment verification; physical logo quality and new full-flow timing await Cam's next Start.
- Private capture and flash outputs: ignored `firmware/build/story020-boot.log` and `story020-flash.log`. All host suites and whitespace checks pass. No commit or push.

### 2026-10-26 — Human feedback: center, signature and restore preamble timing

- Cam: "The logo is good, but it needs to be centered." Requested exact signature "-- Death, the Fortune-Teller" and printing at the fortune preamble, since the smaller logo is fast enough.
- Retain accepted 192x177 pixels. Send ESC a1 after ESC@ and before raster rows; existing post-raster ESC a0 restores left-aligned text. This adds only3 wire bytes and no padding rows/columns. Supplied CSN-A1X-1.pdf p22 describes ESC a1 as middle alignment for all line data; pp32–33 document GS v0 raster output. Applicability to this printer's actual raster alignment still needs a paper check.
- Restore original fortune generation at preamble entry and one print250ms after its audio starts, with the original missing-clip fallback. Tests explicitly assert zero prints throughout Welcome and until250ms into the preamble, plus no duplicate/early goodbye when printing completes during speech.
- Signature is27 ASCII characters and fits the existing32-column line without truncation. Tests check exact signature and the center-before-raster/left-before-text command sequence, including cancellation packet boundaries.
- Revision Death and all general host sanitizer suites PASS; native build and whitespace checks PASS. Application-only flashed at0x30000 with data hash verified; binary SHA256 `22b54a3d1e4e7264bcbe583a0ca9b702099003751b5dac56d2adcc482b3a6123`. Boot reports all9assets, speaker1/stream1, printer ready1/busy0, show Waiting. Private evidence is `firmware/build/story020-revision-{death-tests,host-tests,build,flash,boot}.log`. No print triggered during deployment check; leave story open until final physical layout acceptance. No commit/push.

### 2026-10-26 — Human acceptance

Cam: "Looks great!" and "the printing is excellent and the timing is perfect now." Centered logo/signature/timing accepted; Story020 complete. Pre-existing missing sentence spaces are a separate Story021 fix.
