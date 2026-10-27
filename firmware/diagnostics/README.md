# Native SDMMC register capture

`DEATH_SD_REGISTER_CAPTURE` defaults OFF. Enable with the native ESP32 build
only; combining it with `DEATH_SD_SPI_TRANSPORT` is rejected. This diagnostic
does not establish a fix or authorize a hardware flash.

At configure time, `generate_sd_capture.py` verifies SHA-256 for five exact
ESP-IDF 5.5.4 inputs, checks unique insertion anchors, and generates local
copies of the driver sources/private header in the build directory. CMake
replaces exactly the host and transaction sources and adds the small capture
implementation. Installed SDK files remain untouched. Changes to pinned inputs
trigger reconfigure and a hard failure until deliberately reviewed.

The stock driver, interrupt mask, FIFO reset, DMA stop, and error propagation
remain intact. Hooks add observations only. A shared internal-RAM spinlock
protects command metadata, capture, and teardown across task/ISR cores. Arming
requires matching slot/opcode and a hardware data-expected command; it brackets
the existing command-register write. This prevents publishing partial metadata
or treating pre-issue ISR activity as a new command. It does not prove an old
pending peripheral flag belongs to the newly issued command: the start snapshot
must be inspected for that case.

The ISR records the first masked data error for each active data transaction,
before the stock ISR clears flags or the task resets FIFO. It never allocates,
logs, or accesses files. Full RINTSTS, MINTSTS, IDSTS, STATUS, TCBCNT, and TBBCNT
are recorded at issue and at error. Register reads are sequential, not an
atomic hardware snapshot. Raw disabled bits, especially FRUN, may be sticky;
compare start and fault values rather than treating every set bit as fresh.

Afterward, task-context `sdmmc_req` warning records emit three joined lines:
metadata (`seq`, slot, command, argument, length, block length, return/error),
`start`, and `fault-isr`. Expected response-only initialization timeouts cannot
trigger a data capture. One successful CMD17/18 emits `good-task` per boot,
including the same register set at task completion. It may be a FAT metadata
sector read; opcode/argument/length distinguish it. Its completion flags may
already have been cleared by the normal ISR, unlike the pre-clear fault image.

This is a timing perturbation: short critical sections and post-transaction
logging add work. The interrupt remains non-IRAM-safe exactly as in the stock
driver; this does not test cache-off interrupt service or fix starvation.

Run the bounded generator tests without building firmware:

```sh
IDF_PATH=/Users/cam/esp/esp-idf-v5.5.4 python3 firmware/diagnostics/test_generate_sd_capture.py
```

The tests validate exact generated-source scope, first-error hook ordering,
command issue bracketing, unchanged private header, refusal of mutations to
each pinned input, unique anchors, and deterministic repeated configuration.
Firmware compilation and physical qualification remain separate checks.
