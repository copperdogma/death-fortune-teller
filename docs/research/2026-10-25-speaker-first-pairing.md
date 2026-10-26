# First speaker connection after native migration — 2026-10-25

## Problem class

Fresh persistent state versus reconnecting to a known Bluetooth peripheral.
A powered speaker can accept a connection by a remembered address while being
absent from name discovery. The native migration created fresh NVS, so it did
not carry the Arduino ESP32-A2DP address cache into the native radio module.

## Bounded research and comparison

- Death `skull_radio.cpp` remained byte-identical to the working native TwoSkulls source at57ff5cc; checked with cmp.
- [Espressif Classic GAP reference](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/bluetooth/esp_gap_bt.html) distinguishes inquiry/name discovery from connecting to an address.
- Installed Arduino ESP32-A2DP `BluetoothA2DPCommon.cpp` uses the `connected_bda` NVS namespace. Native TwoSkulls stores a successfully connected address bound to the configured name in `skull_radio/speaker`. Empty native NVS has neither a native target cache nor the original host bond state.
- TwoSkulls research record `docs/research/2026-10-15-radio-memory-debugging.md`, “Reconnect semantics after audible success,” already documents this first-discovery versus saved-address distinction.

## Experiment and result

Initial and explicit retry inquiries ran to completion and found no configured
speaker. Cam confirmed it was powered on only. Requested one press of its
Bluetooth pairing button, then captured a bounded retry window without changing
firmware. Exact `JBL Flip 5` discovered, authentication status0, A2DP connected,
media start acknowledged, speaker1/stream1 with increasing PCM callbacks. Native
radio logged “Remembered configured speaker for reboot reconnect.” Diagnostic
tone accepted; Cam confirmed “It paired!” and “I heard a beep.”

## Decision and remaining uncertainty

Keep the inherited radio implementation unchanged. Document first pairing after
native migration, followed by native address persistence. Automatic reconnect is
implemented and the address save is demonstrated; repeated reboot reconnect was
not retested in this experiment. Audible diagnostic tone confirms the radio and
synthetic PCM path, not SD-backed speech. This boot still failed startup PCM SD
reading, so SD/audio and touch remain separate open issues. The user accepts an
isolated old boot failure only if present operation is demonstrated; do not treat
historic logs alone as proof of a current fault or successful file playback.

## Evidence

Raw private capture: `firmware/build/validation/death-first-flash-20261025/speaker-pairing-watch.log`.
It includes inquiry, exact candidate, authentication, media start, address-save
and tone-acceptance events. No firmware flash, NVS erasure, card changes or
printer jobs occurred in this investigation.
