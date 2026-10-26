# Pirate skull header and UART completion — 2026-10-25

## Bitmap and protocol

The verified card backup and repository `printer/logo_384w.bmp` are identical:
SHA256 `69eb0f3c6f47d07288b116f5454d569240925d2503628c212390b015ccc51710`.
It is384×353 pixels, uncompressed1-bit BMP, with palette0white/1black and
bottom-up rows. The embedded flash data reverses rows, preserves MSB-first pixel
order, and excludes BMP headers/padding. Packed16944-byte pixel SHA256 is
`9e71966de442fad40971c4e5958dae34c9e9951c112a01fed59694b950f43370`;
FNV1a `f85311c2`. Existing `logo/PirateLogo.h` does not match this card BMP and was
not used. Visual inspection confirms the pirate hat/skull/crossed swords.

The supplied Cashino CSN-A1X manual pp32–33 documents GS v0 raster printing,
48-byte maximum width for58mm, black bits1, and automatic feed equal to image
height independent of text line spacing. The implementation sends one complete
56-byte command per row (8-byte header+48data). This keeps cancellation between
complete commands, rather than abandoning a multi-row payload and corrupting
interpretation of the next fortune. A normal LF separates the finished header
from text. At9600baud the353packets require about20.6seconds of wire time.

## First hardware result and bounded UART experiment

Parent observed the first logo job latch a UART failure roughly800ms after
queueing; previous text-only fortune output was readable. No precise failing
API result was logged. Do not label the cause established from that timing.

[Espressif's exact5.5.4 UART reference](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/peripherals/uart.html)
explains that a positive TX-buffer size queues bytes for a later ISR, whereas a
zero TX buffer makes write_bytes push them into the hardware FIFO before return.
The installed `uart.c`1579–1624 confirms these branches. Its wait_tx_done at1486–1547
checks hardware idle and otherwise waits for an ISR semaphore; it does not itself
inspect pending software-ring contents in the initial idle test.

Decision: retain the dedicated worker and64-byte packet bound, but set TX ring to
zero. The existing explicit drain after every packet means a second software
queue is unnecessary. Packets fit the ESP32's128-byte FIFO. Keep baud and drain
timeout unchanged; no retries. Add failure-only logs of accepted byte count,
actual drain result, timeout ticks, elapsed microseconds, cancellation status,
and failing logo row. This is a source-supported controlled experiment, not proof
that the initial hardware failure was a queue/idle race.

## Local checks and remaining uncertainty

All host sanitizer suites pass. Printer tests verify every binary raster packet
(including NULs), fixed pixel checksum, exact sequence before fortune text, full
packet completion during cancellation, a subsequent successful print, queue
limits, and latched drain failure. The install stub now asserts TX ring0.
Physical header output and the actual failure diagnostics still require parent
bench verification. No SD assets or shared SDK sources were modified.
