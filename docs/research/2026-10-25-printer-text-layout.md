# Explicit text layout and paper clearance — 2026-10-25

## Observation and evidence boundary

After receiver pacing produced a clean pirate logo and readable fortune, Cam reported inconsistent signature size and inadequate blank space before/after it. The photo does not establish the printer's exact font state. The firmware sends no signature-specific font command, and the vendor does not document GS v0 changing text font or spacing. Do not claim the photograph proves such a transition.

## Primary command definitions

The supplied Cashino CSN-A1X manual (`docs/hardware/.../CSN-A1X-1.pdf`, extracted locally as `/tmp/death-csn-manual.txt`) documents:

- LF prints and advances according to current line spacing (p12).
- ESC J n prints pending content and advances n physical dots when the print buffer is empty (p13). One dot is0.125mm.
- ESC2 selects30-dot default line spacing (pp14–15).
- ESC M0 selects font A12×24; ESC!0 clears alternate font/bold/double-height/double-width/underline (p16).
- GS!0 selects normal character width/height (p17).
- ESC a0 selects left alignment (p22).
- GS v0 performs image-height feed independently of text line spacing (pp32–33). It does not promise a changed font/spacing state afterward.

## Controlled correction

After all complete, paced raster rows, send ESC!0, GS!0, ESC M0, ESC2 and ESC a0. No second ESC@ is sent, because clearing the printer buffers at the image/text boundary could discard pending printing. Body and signature share these explicit normal font settings.

Keep the formatter's logical blank lines unchanged. Transport maps each empty line to ESC J30, followed by100ms estimated physical-feed processing time in two50ms cancellation-checked quanta. Nonempty text still ends in LF under explicit30-dot spacing. This produces a physical30-dot gap before the signature and five30-dot feeds after its ending LF. The five footer blanks are the existing two plus the requested three; their added physical feed totals150dots(18.75mm). The image/text separator also uses one explicit physical line after normalization.

The existing50ms receiver-processing gap after every logo row is unchanged. Font commands, packet sizes, cancellation framing, baud and UART configuration otherwise remain unchanged. This correction asserts intended layout and paper movement; it does not prove the initial layout failure's cause.

## Local validation and remaining bench check

Production printer tests pass with AddressSanitizer/UndefinedBehaviorSanitizer and warnings-as-errors. Tests preserve the exact353 binary raster packets, check the complete14-byte text-normalization sequence and physical-feed/text order, retain logical formatter expectations, count the processing quanta, and cancel in the first physical-feed quantum to prove no second quantum or following text is emitted. Prior mid-raster and receiver-gap cancellation checks remain.

No hardware test or flash was performed by this task. Physical signature appearance, blank-space dimensions and repeat-print consistency still require confirmation on the actual printer.

## 2026-10-26 — Center the smaller raster only

For Story020, the owner accepted the half-size logo and requested centering. The supplied manual p22 defines ESC a1 as middle alignment for all line data. Apply it after the initial ESC@ and before the177 one-row GS v0 packets; keep the existing ESC a0 before text. This preserves the accepted bitmap bytes and avoids padded raster columns increasing serial time. Exact command ordering is covered by the production printer host test; Cam subsequently confirmed the centered logo and signature look great, with excellent printing and perfect timing (Story020). Signature becomes the requested27-character `-- Death, the Fortune-Teller`, within32 columns.
