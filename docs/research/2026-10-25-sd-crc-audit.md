# Bounded SD CRC audit — 2026-10-25

## Problem class and evidence boundary

This is a firmware/driver migration regression investigation with a previously
working card/board. Small metadata reads often succeed, but long native audio
reads report0x109 and stop. Neither occurrence nor CRC alone identifies a faulty
card. No card writes, resets, flashing or hardware changes were performed by
this audit; HomeKit commissioning was in progress.

Parent-provided bench evidence: native20MHz/1-bit startup reads repeatedly failed;
a current SD-backed speech clip delivered56446frames before CRC and cancellation.
Removing touch did not fix it. Startup4096-byte internal-DMA staging still failed
after36864/425700bytes. A10MHz variant also failed, including a header read. Those
results make further blind clock reductions or retry accumulation low-value.

## Concrete old/new difference: frequency units also select card mode

Local original `src/sd_card_manager.cpp` declares `SD_MOUNT_FREQUENCY=20000000UL`
and passes it to `SD_MMC.begin`, describing it as Hz. The installed Arduino2.0.17
implementation at `.../framework-arduinoespressif32/libraries/SD_MMC/src/SD_MMC.cpp`
assigns that argument directly to `host.max_freq_khz`. Thus the old request was
**20,000,000kHz**, not20,000kHz. The new native source requests20,000kHz (now the
10,000kHz experiment). The comments saying these are equivalent20MHz setups do
not establish equivalent runtime card configuration.

The official [ESP-IDF4.4.7 SD initialization source](https://github.com/espressif/esp-idf/blob/v4.4.7/components/sdmmc/sdmmc_sd.c#L195)
checks the requested upper frequency against20MHz before trying the high-speed
card access mode. A successful high-speed switch selects40MHz; unsupported cards
fall back to20MHz through the [common initialization source](https://github.com/espressif/esp-idf/blob/v4.4.7/components/sdmmc/sdmmc_common.c#L143).
The [old host clock driver](https://github.com/espressif/esp-idf/blob/v4.4.7/components/driver/sdmmc_host.c#L181)
uses a40MHz clock for the high-speed request. The original oversized request
therefore entered a different card-mode-selection branch than the native20/10MHz
requests. **Actual old40MHz operation remains conditional on card support.**

The installed5.5.4 owning source `components/sdmmc/sdmmc_sd.c`,
`sdmmc_enable_hs_mode_and_check`, retains the same initial low-frequency versus
high-speed selection distinction. Native20/10MHz does not try high-speed card mode.
The960MB card could be an older non-HS card; capacity alone cannot settle this.

## Other comparisons that do not justify a new workaround

- Old Arduino explicitly uses `host.flags=SDMMC_HOST_FLAG_1BIT`; native host starts
  with the SDK default and slot width1. IDF5.5.4 `sdmmc_fix_host_flags` reconciles
  width flags to the configured slot width, so no demonstrated accidental4-bit
  transfer follows from that superficial source difference.
- Old and new classic-ESP32 IOMUX configuration both use drive strength3.
  There is no identified migration change supporting an arbitrary drive-strength
  adjustment.
- IDF5.5.4 SDMMC explicitly bounces external-RAM reads through DMA-capable memory.
  Direct PSRAM destination is supported through that fallback; it is not proof
  that the application violated the driver contract. The failed staging result
  further weakens that explanation as the sole cause.
- The reported0x109 is a transfer-level CRC error; invalid WAV classification
  after a failed header read does not prove an authored-format defect.

## One next discriminating experiment

After HomeKit commissioning is no longer active, test an explicit
`SDMMC_FREQ_HIGHSPEED` (40,000kHz) native build, retaining1-bit mode and the other
current settings. This tests the old request's high-speed **card-mode branch**,
not a generic faster-is-better hypothesis. At mount, log `sdmmc_card_print_info`
or the equivalent CID/name, `is_mmc`, SCR spec, card max frequency and actual
`real_freq_khz`. Then read the full same startup clip and compare its checksum
with the checked-in asset; require full playback rather than a short audible
fragment. No retries should hide the first result.

If the card rejects high-speed and falls back20MHz, record that the experiment
did not change card mode/clock; do not claim either a40MHz failure or success.
If high-speed reads pass, return to20MHz once for a bounded A/B before adopting
it as the compatibility setting. If it does not discriminate, stop clock tuning
and use a preserved Arduino-image/read-only baseline comparison with native
state backed up first. That larger comparison is a separate decision, not
permission granted or performed by this audit.

## Status

The frequency-unit mismatch and mode branch are verified by local owning source
plus official versioned SDK source. The physical card's negotiated old/native
modes have not been measured here. No root-cause or hardware-health conclusion
is established. Parent should apply a small device experiment before changing
the production default on the basis of this finding.
