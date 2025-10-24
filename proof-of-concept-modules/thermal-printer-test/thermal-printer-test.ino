// CSN-A1X Thermal Printer — Raster Bitmap Printing (ESP32)
// Wiring: Printer TXD -> GPIO19, Printer RXD <- GPIO18, GND common
// Power: Printer 5V >= 2A (or 9V). Baud: 9600 8N1.

#include <HardwareSerial.h>
#include <pgmspace.h>
#include "PirateLogo.h"

HardwareSerial PRN(2);

// --- Pins & serial ---
static const int PIN_RX = 19;     // ESP32 RX  <- printer TXD
static const int PIN_TX = 18;     // ESP32 TX  -> printer RXD
static const long PRN_BAUD = 9600;

// --- Printer limits (your unit) ---
static const uint16_t PRN_MAX_W = 384;  // confirmed max width (dots)

// --- Core helpers ---
void escInit()                { PRN.write(0x1B); PRN.write('@'); }
void feedLF(uint8_t n)        { while (n--) PRN.write('\n'); }
void prnBegin()               { PRN.begin(PRN_BAUD, SERIAL_8N1, PIN_RX, PIN_TX); }

// printBitmap: 1bpp packed bitmap (MSB=left, 1=black)
// If inProgmem=true, data is in PROGMEM; otherwise from RAM.
// Auto-clamps width to 384 and optional centers by left padding.
void printBitmap(const uint8_t* bitmap,
                 uint16_t w_px, uint16_t h_px,
                 bool center = true,
                 bool invert = false,
                 uint8_t mode = 0,
                 bool inProgmem = true)
{
  if (!bitmap || !w_px || !h_px) return;

  // Clamp to printer max width
  uint16_t w = (w_px > PRN_MAX_W) ? PRN_MAX_W : w_px;

  // Byte widths
  const uint16_t src_row_bytes = (w_px + 7) >> 3;   // in source bitmap
  const uint16_t out_row_bytes = (w    + 7) >> 3;   // what we'll send

  // Optional left pad (bytes) for crude centering at full-width framing
  uint16_t pad_bytes = 0;
  if (center && w_px < PRN_MAX_W) {
    const uint16_t total_bytes = (PRN_MAX_W + 7) >> 3; // 384 -> 48 bytes
    pad_bytes = (total_bytes - out_row_bytes) / 2;
  }

  // Header: GS v 0 m xL xH yL yH
  const uint16_t send_row_bytes = pad_bytes + out_row_bytes; // no right pad
  const uint8_t xL = send_row_bytes & 0xFF;
  const uint8_t xH = (send_row_bytes >> 8) & 0xFF;
  const uint8_t yL = h_px & 0xFF;
  const uint8_t yH = (h_px >> 8) & 0xFF;

  PRN.write(0x1D); PRN.write('v'); PRN.write('0'); PRN.write(mode);
  PRN.write(xL); PRN.write(xH); PRN.write(yL); PRN.write(yH);

  // Stream each row: left pad (white), then clamped payload bytes
  for (uint16_t y = 0; y < h_px; ++y) {
    // left pad as white (0 bits)
    for (uint16_t i = 0; i < pad_bytes; ++i) PRN.write((uint8_t)0x00);

    const uint32_t row_off = (uint32_t)y * src_row_bytes;
    for (uint16_t i = 0; i < out_row_bytes; ++i) {
      uint8_t b = inProgmem ? pgm_read_byte(bitmap + row_off + i)
                            : *(bitmap + row_off + i);
      PRN.write(invert ? (uint8_t)~b : b);
    }
  }
}

// --- Demo bitmap (16x16 smile) — replace with your own ---
// Format: packed 1bpp, MSB=left, 1=black. Each row = ceil(16/8)=2 bytes.
#define DEMO_W 16
#define DEMO_H 16
const uint8_t PROGMEM DEMO_BITMAP[] = {
  0b00000000,0b00000000,
  0b00111111,0b11000000,
  0b01000000,0b00100000,
  0b10011001,0b00010000,
  0b10100101,0b00001000,
  0b10000001,0b00001000,
  0b10000001,0b00001000,
  0b10100101,0b00001000,
  0b10011001,0b00001000,
  0b10000001,0b00001000,
  0b10100101,0b00001000,
  0b10011001,0b00010000,
  0b01000000,0b00100000,
  0b00111111,0b11000000,
  0b00000000,0b00000000,
  0b00000000,0b00000000,
};

void setup() {
  Serial.begin(115200);
  prnBegin();
  delay(200);

  escInit();
  printBitmap(PirateLogo, PirateLogo_W, PirateLogo_H, /*center=*/true, /*invert=*/false, /*mode=*/0, /*inProgmem=*/true);
  feedLF(3);

  Serial.println("Bitmap demo sent.");
}

void loop() { /* no-op */ }