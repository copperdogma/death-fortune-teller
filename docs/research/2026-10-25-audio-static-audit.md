# Native audio static: volume semantics and next experiment

Problem class: audible distortion after underruns are eliminated can come from source PCM clipping, DAC/speaker overload, transport corruption, or the target speaker's own amplification. The current evidence supports an immediate, low-risk amplitude A/B before changing the transport again.

## Source findings

- The project-local ESP32-A2DP implementation defines `set_volume` on the AVRCP scale 0–127 (`.pio/libdeps/esp32dev/ESP32-A2DP/src/BluetoothA2DPCommon.h:222-228`). Its default volume controller maps that value through an exponential curve, then scales each signed PCM channel by `volumeFactor / 4096` and clips to int16 (`A2DPVolumeControl.h:40-60, 123-153`). At input 100, this curve is about 41.5% amplitude (roughly -7.6 dB), not 100% amplitude. This is a library source fact, not an assumption about a physical speaker.
- Legacy app config `speaker_volume=100` is passed directly to A2DP's `set_volume` (`src/app_controller.cpp:540`, `src/bluetooth_controller.cpp:79-83`), so it meant an A2DP digital attenuation setting about -7.6 dB plus whatever speaker volume the sink had. The native firmware instead treats the same config as linear PCM gain (`firmware/main/skull_startup.cpp:70-79`; `firmware/main/skull_audio.cpp:155,206-208`); 100 means unity gain. This is a substantial semantic change and gives the native stream much greater amplitude than legacy at the same config.
- Native `provide` copies PCM without further scaling, so near-full-scale source samples stay near full scale. The observed stream peaks at 32761/32763 are consistent with source peaks around the signed 16-bit ceiling; that alone does not prove digital clipping. The one or few sample count differences versus reference are too small to diagnose static and may reflect a different card asset or concurrent capture.
- The prior stream test reports 106,425 frames and zero underruns after the clock/FIFO adjustment. Therefore a repeatable distortion test should keep that known-good stream path fixed while changing only gain.

## Decision: next test

Use the already planned warm-clip A/B at 100% and 40% native gain, same speaker, same speaker volume, same short clip, no other code/transport changes. Capture callback PCM peak and count samples at either int16 rail (and CRC/SHA of captured stream, if available) for both runs. Listen for whether static falls with amplitude. If it does, the native gain/config semantic mismatch or downstream overload becomes the lead; next test should vary the speaker's physical volume independently. If static does not change while PCM is clean and underruns remain zero, source gain is not the cause and investigation should move to A2DP sink/codec/radio or speaker behavior. A 40% test reduces peaks to about 13,100 and is much more discriminating than a tiny gain adjustment.

## Uncertainty

The former Arduino source checkout was not present in this workspace, so legacy behavior is inferred from this project's `set_volume(config)` call and the exact currently installed ESP32-A2DP library implementation. Confirm the historical library version if exact old firmware parity becomes necessary. Neither source inspection nor PCM metrics can establish whether audible static originates inside the Flip 5; the controlled listening A/B is required.

## Further evidence and diagnostic split

The32-block image completed the short streamed clip with zero underruns, but Cam
still reported static. Thus neither ring starvation nor a successful source-frame
count alone settles the audible issue. Direct playback's raw peak was~32761,
while the checked-in fixture peak is19538; the physical card may contain a
different render, so do not assume repository bytes equal the installed asset.

However the unchanged card's startup raw FNV changed between two native boots
(`61fd55e1` versus `c5bc6ef6`). Those hashes are computed before gain. This is a
repeatability question independent of volume. Added diagnostic-only comparisons:
startup staging bytes before PSRAM copy, hash after the PSRAM copy, and two
read-only sector-aligned internal-DMA probes. The latter do not need an attached
speaker and never write the card. Tests establish the hash implementation against
the actual repository fixture (`d6d8b5d5`,425700bytes) and repeated reads.

Decision rules for the next device observation:

- Equal staging and PSRAM hashes rule out that copy as the observed difference.
- Equal repeated direct-probe hashes but a different stdio/staging hash localize
  the issue to read-path/stream handling, rather than assuming a bad card.
- Equal hashes across all physical read paths, differing only from the repo,
  identify a different installed render; assess its level/quiet replay separately.
- Differing direct-probe hashes leave read integrity unresolved; do not hide it
  with gain changes or additional buffer growth.

The quieter40% replay is not yet judged. The speaker became unreachable during
this diagnostic episode, with page timeouts; Cam was asked to confirm power.

## Device result of the diagnostic split

In probe-boot.log, startup staging and PSRAM both hash1044401d. Two direct aligned
internal-DMA reads both succeeded for425700bytes but hashedc9c22729 and47fae215;
peaks32767/32763, nonzero212578 in each. Earlier boot hashesc5bc6ef6/61fd55e1 also
differ. This does not establish repeatable raw data; the PSRAM copy is not the
identified cause. Lower40% gain was heard as quieter but still staticky. Do not
retain gain reduction as a claimed fix or inflate buffers further on this basis.

The longer aligned-reader welcome clip completed540540frames with0underruns;
read/DSP totals3.315s/2.159s over12.26s audio, so this pass supplies sustained
throughput evidence rather than only a short prebuffer pass. User still heard
some static. Next step is independent read-only comparison from the computer's
card reader, not a claim that a card which worked previously must be bad.
