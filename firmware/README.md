# Death 2026 native firmware

This is the active single-ESP32-WROVER rebuild, tracked by
[Story015](../docs/stories/story-015-single-wrover-2026-rebuild.md).
It uses the native TwoSkulls implementation at `57ff5cc` as its base: ESP-IDF5.5.4,
ESP-Matter1.5.0, one retained BTDM/Bluedroid host, PSRAM PCM producer, onboard SD,
and guarded LEDC updates. The source project is unchanged. See
[provenance](../docs/research/2026-10-25-native-base.md).

Death owns its own Matter accessory with Start, Dead Stop and Waiting endpoints,
A2DP speaker, jaw/lights and printer. Touch input is retired. The new
application does not initialize a C3 UART or the TwoSkulls show/peer discovery.
The original Arduino `src/` and PlatformIO configuration remain rollback/reference;
`pio run` builds that historical firmware, not this replacement.

## Build and host checks

```sh
. /Users/cam/esp/esp-idf-v5.5.4/export.sh
idf.py -C firmware -DIDF_TARGET=esp32 build
idf.py -C firmware size
python3 native_scripts/run_host_tests.py
python3 native_scripts/run_death_tests.py --idf-path "$IDF_PATH"
```

On another machine install ESP-IDF5.5.4 and its classic ESP32 toolchain. The
manifest/lock pin managed dependencies. Generated config, build and component
files are ignored. The checked Bluedroid correction stays in a build-local copy.
`sdkconfig.defaults` is for a fresh build; when changing existing generated config,
update it deliberately or regenerate it from defaults. The root plus three Matter
endpoints require `CONFIG_ESP_MATTER_MAX_DYNAMIC_ENDPOINT_COUNT=4`. Boot directory
path buffers live in PSRAM, allowing a6KiB main-task stack that is freed when
app_main returns. No OTA is
provided by this inherited single-factory-app partition layout.

## Card and wiring

Copy the contents of `sd-card-files/` to a FAT card. Firmware reads existing files;
it never formats a failing card or writes assets. Prefer `/config/config.txt`,
with `/config.txt` fallback. No `role` is required. Set the actual `speaker_name`,
volume and Death-specific servo calibration. Configured pulse endpoints keep
ServoESP32's original0–180° mapping and3° margin inside Death's0–80° travel;
`servo_reverse` reverses physical direction. Missing calibration uses a narrow
1400–1600us pulse range, not another prop's full-range calibration.

- SD: onboard1-bit SDMMC, CLK14/CMD15/D0=2,20MHz matching qualified TwoSkulls.
  Earlier20/10MHz CRC failures led to a40MHz experiment; the controlled return
  to20MHz now boots/streams without CRC in bounded tests, but read integrity
  and residual audible static remain under investigation. No format or asset writes.
- Jaw signal23, eye LED32, mouth LED33.
- Printer: UART1 TX18 to printer RX, RX19 from printer TX,9600baud8N1.
- Remove the C3 Super Mini and its GPIO21/22 UART connections from the new build.
- Preserve separate suitably powered servo/printer supplies and common ground;
  this firmware change does not establish electrical isolation or wiring safety.

The checked-in card already has `audio/initialized.wav` and the welcome,
finger_prompt, finger_snap, no_finger, fortune_preamble and goodbye categories.
The catalog recursively reads up to32 valid PCM stereo44.1kHz16-bit WAVs, with
bounded directory depth. Invalid clips are diagnosed and skipped. It does not
convert audio. Fortune JSON uses version1/templates/wordlists with `{{tokens}}`.
The configured fortune path is tried first; `/fortunes/little_kid_fortunes.json`
is tried next. If both SD loads fail, an embedded copy of the verified card
template supplies real fortunes; its source is logged at boot. The embedded
copy preserves the card’s exact727-byte template and wordlists. This protects
fortune generation from an SD load failure; it does not fix SD audio reads. `/audio/fortune_told` falls back to `/audio/goodbye`.

The printer uses bounded text jobs, sanitizes control/non-ASCII bytes, preserves
newlines, wraps at32 columns and waits for wire drain between paced lines. The
original384x353 pirate-skull bitmap is embedded in flash and printed before
each fortune as complete56-byte raster-row packets. At9600baud the header
takes about38s with conservative receiver pacing; the flow waits for printing to finish. Row boundaries allow
cancellation without leaving a partial image command for the next job. Normal
12x24 text and30-dot spacing are explicitly restored after the image. Empty
lines use physical30-dot feeds, including five lines after the signature (the
original two plus three extra), rather than depending on empty-buffer LF. The
print-wait watchdog allows60s after speech for printing to finish.
Transport completion does not prove paper output or printer readiness. A cancel
cannot retract already transmitted/printed text. There is no verified BUSY/RTS
or paper-status contract yet.

## Runtime and diagnostics

Three Home-compatible On/Off controls implement the demo flow:

- **Start (endpoint1):** Welcome → fortune preamble/generation and one print →
  goodbye → configured cooldown → Waiting. Start remains On through cooldown;
  any additional Start is ignored until the complete sequence finishes.
- **Waiting (endpoint3):** Idle-state indicator. Tapping it has no action.
- **Dead Stop (endpoint2):** Stops startup/show/tone and cancels printer work,
  immediately reports Waiting and clears Start, with no cooldown. Start remains
  gated briefly until worker/provider ownership has been safely released.

No Near/finger/sensor gate remains. Turning Start Off is not a Stop command.
Missing clips advance; radio/SD errors cancel and retain the normal cooldown.
Only explicit Dead Stop skips cooldown. Already transmitted speaker or
printer bytes cannot be recalled. Labels are advertised as semantic tags, but
Home may retain generic outlet names; use the endpoint mapping and a live tap
check to identify/rename them. Existing endpoint1/2 identities and fabric are
preserved, and endpoint3 is added. The separate Box of Death Home automation
should turn **Start On** when its button is pressed; no controller changes are
part of this firmware.

115200baud console:

- `status`: resource/audio/printer transport telemetry.
- `sd-check`: two read-only raw PCM probes while idle; compares repeatability
  independently of the speaker without changing files.
- `sd-window`: two internal-buffer reads compared with the embedded initialized.wav
  reference at fileoffset28672; diagnostic reference is specific to the supplied asset.
- `start` (or `trigger`): same Start gate as Matter.
- `waiting`: report the current state without triggering anything.
- `stop` (or `cancel`): Dead Stop, including startup/tone and jaw closure.
- `play /audio/<path>.wav`, `asset /audio/<path>.wav`, `tone`, `jaw-test`.
- `gain 0..100`: diagnostic gain for future streamed files, accepted only idle;
  does not change the card, NVS or the already-preloaded startup clip.
- `retry`: bounded remembered-speaker reconnect/discovery episode.
- `heap-start`, `heap-stop`: bounded resource observation window.
- `pairing`: onboarding codes/window for an uncommissioned Death only.
- `reboot`: refused while files/show/audio/printer are active.
- `factory-reset CONFIRM`: explicitly destroys this board's Matter/network state;
  refused during show/startup/printer work. Ordinary updates preserve NVS.

The touch module and its diagnostics are removed; old sensor keys on the card
are accepted as unused legacy settings. Inherited paired-skull scripts and tests are base regression/reference tools;
`paired_bench.py` is not a Death commissioning procedure.

## First flash and bench acceptance

Do not paste old Arduino/C3 offsets. Before changing hardware firmware, capture
and verify a full4MiB original flash backup and retain the old card/config.
This native layout changes the partition table to0xC000, NVS0x10000/64KiB and
factory app0x30000. Preserve the backup, then deliberately initialize only the
new NVS region during the first migration; the app never erases NVS automatically.
Use this build's generated flash_args/idf.py flash. Later app updates use
`idf.py -C firmware app-flash` and preserve the native fabric. Commission this
as a new Death accessory using its actual printed setup payload; never copy C3
fabric data. Identify/name the Start, Dead Stop and Waiting outlets using boot
endpoint IDs and bind the Home button automation to Start On. The SDK uses prototype/test attestation.

The first native migration starts with an empty speaker-address cache. Put the
configured speaker in Bluetooth pairing mode once and use console `retry` if
initial discovery already ended. After successful connection, firmware saves its
address for later reconnects. Merely powering on a previously paired speaker may
not make it discoverable to this fresh firmware. Cam confirmed the first native
connection and audible diagnostic tone on2026-10-25; see the
[speaker investigation](../docs/research/2026-10-25-speaker-first-pairing.md).

The native application has been flashed and speaker pairing/tone confirmed.
HomeKit commissioning and command receipt are confirmed. Audio underrun and
repeat-boot qualification are still in progress. Bench acceptance:

1. Confirm actual board has the onboard slot and PSRAM; verify boot PSRAM/resource logs.
2. Check closed/open servo motion against Death calibration before full motion tests.
3. Commission/reboot, verify both Home endpoints return Off and trigger the correct phase.
4. Turn Start On and verify automatic welcome/fortune/print/goodbye/cooldown. Test repeat rejection and Dead Stop.
5. Confirm readable paper output, long/newline fortunes, busy rejection and cancellation.
6. Repeat complete flows while monitoring watchdogs, underruns and internal heap/stack minima.
7. Exercise speaker loss/retry, missing assets and printer unplug/paper-out honestly;
   transport-only status cannot assert paper health.
8. Human verifies audible jaw sync and full physical behavior before marking Story015 Done.

## Current audio qualification notes

The controlled baseline restores the qualified TwoSkulls8x20ms PSRAM blocks,
retained8192-bytePSRAM stdio buffer and20MHz SD clock. Only diagnostic gain/hash
API additions remain in the stream implementation. Startup retains2s preroll.
Short speech was judged clean; long speech improved but still had some static,
with zero underruns for both. Data-integrity diagnostics demonstrated differing
bytes in internal DMA memory even with raw sector reads, before filesystem,
PSRAM staging or Bluetooth encoding. Card-reader reads matched the reference.
Root cause and final audible/reboot acceptance remain open.

`sd-window`/`sd-window-single` compare a fixed4096-byte reference at fileoffset28672.
`sd-check`/`sd-single` hash the complete PCM with multi/single-sector reads.
`sd-raw` checks file-to-sector geometry and bypasses the filesystem for the same
reference window, also recording card R1 status responses. These references are
specific to the supplied initialized.wav. `sd-clock 400|4000|10000|20000` changes
the host clock while idle for diagnostics and resets to20MHz on reboot. Lower
clocks and single-sector reads have not fixed the differing bytes. No card writes.


## Retained fault reports (Story018)

The logger retains32 selected diagnostic lines in a bounded PSRAM ring. A first
error freezes a report (maximum7168bytes); text formatting and persistence happen
outside the log hook. Further errors while a report is pending are coalesced;
the count appears in serial status. CHIP/WiFi/provisioning logs and the card's
configuration-summary line are excluded. Regular serial output continues.

When the full show, audio worker/file, startup, tone and printer are quiescent,
the owner attempts to save a compact report (roughly1KiB) in the separate
`death_fault` NVS namespace, reserving free entries for Matter/system state.
No NVS erase/reinitialization is attempted on failure. Unacknowledged compact
reports are restored on reboot. A crash before persistence or loss of power can
still lose unsaved RAM history; this is not a panic core-dump facility.

SD reports rotate through `/sdcard/diagnostics/fault-0.log` through `fault-7.log`.
A single `pending.tmp` is written, flushed/synced/closed and read back before an
old slot is replaced. These nine known paths have a combined payload cap below
64KiB (filesystem cluster overhead is additional). Other files are not removed.
Failure keeps the frozen RAM/flash report and retries on a60second cadence,
only while idle. The logger never remounts a live card or retries audio reads.
Flash retention/acknowledgement attempts are also rate limited. If internal flash
is unavailable but SD export succeeds, the report can still be acknowledged in
RAM. SD success requires the write and verification, not merely the mounted flag.

Serial commands:

- `status`: includes pending report ID, coalesced errors, retry counts and errors.
- `fault-log`: dumps the pending report from RAM, even if SD export fails.
- `fault-test`: idle-only labelled synthetic error through the production logger;
  it does not corrupt the card or inject an audio failure.

Host suites exercise capture/filtering, frozen snapshots, concurrent messages,
retry cadence, NVS failure/reboot/acknowledgement, rotation and filesystem failure
paths. Hardware export and audible/full-flow acceptance are recorded separately
in `docs/stories/story-018-retained-fault-logs.md`.
