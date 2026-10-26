# Boot/read/reconnect experiments — 2026-10-25

## Problem classes and current scope

Distinguish stale boot warnings from current file-transfer failure; distinguish
radio pairing from reconnecting after a reset while the remote retains its link.
Cam removed the capacitive sensor and deferred trigger/show redesign until stable
boot and HomeKit control. Touch measurements/CLI/config/module/tests are therefore
removed from the native application. Existing show timeout behavior is retained
only as a compatibility placeholder; no new trigger sequence is being designed.
The historical Arduino rollback source and card assets remain intact.

## Owning-source research

- Espressif5.5.4 `components/sdmmc/sdmmc_cmd.c`, `sdmmc_read_sectors`: classic ESP32 cannot DMA directly into external RAM; unaligned/PSRAM reads use an internal DMA buffer and one-sector transactions. This is supported, not itself proof of a bug. A bounded internal multi-sector staging read is an established alternative and a testable transfer-pattern change.
- [Espressif SDMMC example](https://github.com/espressif/esp-idf/blob/v5.5.4/examples/storage/sd_card/sdmmc/README.md) allows lower clock frequency for compatibility. Frequency is a separate experiment; do not change it simultaneously with the first startup-buffer experiment.
- [Espressif boot-mode reference](https://docs.espressif.com/projects/esptool/en/latest/esp32/advanced-topics/boot-mode-selection.html): serial DTR/RTS control reset; some drivers toggle lines on open. The standard two-transistor circuit inhibits reset when both are asserted. HUPCL guidance is Linux-specific, so applicability on this Mac must be measured.
- Previous TwoSkulls radio research explicitly documents HCI0x0B “connection already exists,” the remote-link teardown wait and bounded cached-address retries. Do not confuse this with a missing saved address.

## Local observations before new application upload

1. Reboot check loaded the native saved JBL address automatically. First attempt hit HCI0x0B; three-attempt episode recovered speaker1/stream1 after about83s. An explicit idle console reboot repeated the same initial remote-link collision. The first bounded check ended before its full episode completed; that timeout is not evidence the address cache failed.
2. A steady serial relay with both DTR/RTS asserted observed existing uptime145s rather than a new boot, then live speaker1/stream1. This demonstrated a non-resetting capture setup on this board/host. Keep one port owner for multiple commands.
3. Current `play /audio/initialized.wav` was accepted and actually delivered56446frames before a new0x109 CRC failure. Cam heard “Death initia–” and the cutoff. This is a present SD-backed playback failure, not merely an old startup log. Bluetooth stayed connected.

## First implementation experiment

The startup loader now allocates one4096-byte internal DMA staging buffer,
reads bounded chunks there and copies into the same retained immutable PSRAM PCM.
Failure reports completed bytes and never publishes partial audio. The radio and
streaming PCM producer remain unchanged for this experiment. It also removes
unused touch initialization and its timer/task/GPIO4 configuration.

Production-source host tests verify byte-for-byte startup PCM after preroll,
internal-DMA allocation flags, no callback allocations and destination/staging
allocation failure handling. A short-read fault is not injected. Native build
passed and the application-only upload was hash-verified, preserving native NVS,
speaker cache and any Matter provisioning.

## Evidence and remaining gates

Private captures under `firmware/build/validation/death-first-flash-20261025/`:
`reboot-check.log`, `serial-relay-1.log`, `dma-staging-upload.log`,
`dma-staging-boot.log`. Append actual experimental outcome below; no claim of a
fixed SD path until full native startup and a complete speech clip succeed.
HomeKit still requires actual iPhone commissioning and a command observation.

### First DMA-staging result and next experiment

The4096-byte internal staging variant still failed with0x109 after36864/425700
startup bytes. Removing touch and changing read destination did not fix the CRC.
Native cached speaker reconnect succeeded on its first attempt at~8s in this boot.
Next change only the SDMMC upper clock from20MHz to10MHz, retaining1-bit mode,
pullups, CRC checking and the staging variant. This follows the SDK compatibility
technique; passing would show timing sensitivity, not prove the physical cause.

### Mode comparison revisited after10MHz failure

Independent source audit found old Arduino code passed20000000 to an API taking
kHz, causing the old IDF4.4 implementation to attempt high-speed negotiation.
Whether the actual960MB card used40MHz or fell back20MHz is unknown. The native
20/10MHz requests never attempted that mode switch. The next experiment requests
SDMMC_FREQ_HIGHSPEED explicitly in1-bit mode and prints actual card information,
retaining CRC checks and no card writes. See2026-10-25-sd-crc-audit.md for primary
sources and limits. This is an identified old/new difference, not a claim that
40MHz itself is necessarily more reliable.

Home commissioning succeeded on the10MHz build, and Cam's Home tap reached
native Near endpoint2; old flow correctly ignored Near while idle. Trigger flow
redesign stays deferred. Future app updates preserve NVS/fabric and endpoint IDs.

### High-speed negotiation result

The actual card identifies as APPSD/SDSC960MB and negotiated40.00MHz. Native
startup loaded all425700PCM bytes, then connected the remembered speaker and
delivered the entire clip without a CRC error. Thus this trial really changed
card mode/clock, unlike the unsupported-HS fallback case. Warm direct SD playback
also delivered all106425frames without CRC, whereas20/10MHz variants failed.
Cam confirmed the full phrase on warm replay, but heard static. Status showed
40underruns,121blocks, read total1.637s/max107.72ms, process total305ms,
refill gap max110.31ms. This distinguishes transport starvation from CRC failure.
The initial boot phrase still lost its first word at800ms preroll.

### Buffer experiment informed by TwoSkulls results

Read TwoSkulls'2026-10-15-a2dp-throughput.md and radio-memory-debugging.md. The
inherited two-block yield and240MHz/SBC-O2 provisions are already present and
must stay. A FIFO absorbs jitter but cannot repair a sustained throughput deficit.
Death's old card has much larger measured read stalls than the qualified sibling
cards. Increase PSRAM block count8→32 (160→640ms), preserving the exact worker
burst/yield/cancellation lifecycle. Test the short clip and a~21s clip; failure
on the long clip would demand a throughput/read-path change rather than further
queue inflation. Separately raise boot-only silence preroll800→2000ms to address
the owner's clipped-first-word observation. Host EOF/cancellation/starvation tests
were adjusted to exercise source data longer than the new prebuffer, and passed.
