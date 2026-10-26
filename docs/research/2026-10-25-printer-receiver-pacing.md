# Printer receiver pacing — 2026-10-25

## Problem class and observations

A serial peripheral can accept bytes on the wire faster than it finishes processing commands or physically printing. ESP32 UART drain is not a printer acknowledgement. After two initially successful logo/fortune prints, Cam reported a half-logo followed by gibberish and then a wholly garbled next print. The controller reported completed UART jobs, not transport failures. That pattern is compatible with lost bytes or a printer reset/parser desynchronization; it does not establish which occurred.

The three extra footer lines execute only after the logo, so they cannot directly change its binary payload. This timing does not prove that the firmware change is unrelated to the wider failure.

## Primary sources and local evidence

- [Adafruit's maintained thermal-printer driver](https://github.com/adafruit/Adafruit-Thermal-Printer-Library/blob/master/Adafruit_Thermal.cpp) explicitly throttles for serial transmission **and** estimated physical print/feed completion when no DTR signal is available. Its bitmap path applies a processing timeout after an image chunk. Its own timing constants belong to another printer and are not CSN specifications.
- [Epson's ESC/POS data-processing reference](https://download4.epson.biz/sec_pubs/pos/reference_en/escpos/data_processing.html) distinguishes receive-buffer storage, command processing and real-time commands. This is a protocol reference rather than proof of all third-party firmware behavior.
- Supplied Cashino CSN-A1X manual, extracted locally as `/tmp/death-csn-manual.txt`: pp32–33 support the current GS v0 width48/height1 commands and automatic image-height feeding. The manual provides no processing-time budget for this exact command pattern. TTL pin1 is DTR flow control, pin2 TXD, pin3 RXD and pin4 ground. The installed harness documents TX/RX/ground only; working DTR is not demonstrated.
- Vendor specifications require 5–9V at >=2A. Its real-time DLE EOT3 status reports abnormal-input-voltage/unrecoverable error in bit5 and head-temperature/voltage error in bit6. Reading online status does not establish available receive-buffer space.
- Historical `src/thermal_printer.cpp` uses one full-height raster command transmitted in32-byte chunks, with a4ms application write interval and additional loop scheduling. The new implementation uses353 separate one-row commands. The old source therefore does not demonstrate that continuous new-row commands are safe without a processing gap. Historical Story005 also records correcting printer brownouts through power distribution.

## Controlled decision

Add a **50ms task delay after each complete, UART-drained raster row**, followed by a cancellation check before any further packet. Keep bitmap bytes, baud, software TX-buffer size0, packet sizes and timeout unchanged. This introduces receiver-processing space without resending bytes or leaving a partially specified image command on cancellation.

At9600baud,353x56-byte commands need about20.6s on the wire. The new gaps add17.65s, for approximately38.25s before text. The show printer-wait guard must allow this worst case even when preamble audio is short or missing; root is updating it separately to60s.

This is an empirical, conservative pacing experiment, not a derived timing guarantee or proven cure. An improvement would not distinguish receiver overflow from thermal/power trouble because the gap also lowers print duty. Verified DTR flow control would be preferable to an estimated fixed gap, but must not be enabled against an unverified pin or polarity. Reset the printer before comparing runs so an earlier malformed image does not contaminate the result.

## Local verification

The production printer test passes with AddressSanitizer/UndefinedBehaviorSanitizer and warnings-as-errors. It verifies all353 pacing calls occur after UART drain, preserves the exact binary output, and blocks a known processing gap to prove cancellation emits neither a subsequent row nor fortune text. Existing in-packet cancellation, subsequent-job framing, queue limits and failure-latch coverage remain. No hardware test or flash was performed by this research task; repeated physical print validation remains required.
