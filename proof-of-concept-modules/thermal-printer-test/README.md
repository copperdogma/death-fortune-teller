# Thermal Printer Test

20250830: Aria and I started this to test the thermal printer.
20251011: Finally got it working! See "Setup Summary" below.

Main sections below:
- Setup Summary
- Bitmap Printing (Proof of Concept)
- Test Program


We're following these instruction: https://www.instructables.com/How-to-Use-a-Thermal-Printer-With-ESP32-PNP-500-Tu/

We're hooking up the printer to an ESP32 to make sure we know how to do it and it works. This is an essential step in creating the Death Fortune Teller as it uses a thermal printer to print the fortunes.

  • Baud: 9600 8N1 (don’t change unless you know vendor command).
  • Pins: TX18→RXD(3), RX19←TXD(2), GND↔GND. DTRD unused.
  • Line spacing: default 32 dots ≈ 4 mm (ESC 2).
Change: ESC 3 n (n in dots).
Feed: LF = current spacing; ESC J n = n dots (fine control).
  • Sizes: GS ! n (bit0=height×2, bit4=width×2).
  • 1×1: n=0; 2×1: 0x10; 1×2: 0x01; 2×2: 0x11.
  • Align: ESC a n (0=L, 1=C, 2=R).
  • Status: DLE EOT 1/2/4 → 0x12 = ready. If any differs, block printing.
Door open yields 1A/32/72. EOT 5 not implemented on your unit.
  • Mechanics: ~8 dots/mm. So 80 dots ≈ 10 mm; your measured ~9.5 mm is within tolerance.


The printer is an “Epson-compatible” clones so we can use the generic Epson ESC/POS v2.0 (public spec) command set: 

## Status Bytes (`DLE EOT n`)
| n | Meaning (generic) | Your Observed Value | Notes |
|---|-------------------|---------------------|--------|
| 1 | Printer status | 0x12 (ready) / 0x1A (door open) | Works |
| 2 | Offline status | 0x12 / 0x32 (door open) | Works |
| 4 | Error status | 0x12 / 0x72 (door open) | Works |
| 5 | Paper sensor status | (no reply) | Not implemented |

---

## Core ESC/POS Commands to Validate

| Function | Command (hex) | Example `x` CLI Command | Expected Behavior |
|-----------|---------------|-------------------------|-------------------|
| **Initialize** | `1B 40` | `x 1B 40` | Resets all modes |
| **Feed n lines** | `1B 64 n` | `x 1B 64 03` | _Ignored_ (firmware ignores `ESC d n`) |
| **Feed n dots** | `1B 4A n` | `x 1B 4A 80` | Works (`jd n`) |
| **Line spacing** | `1B 33 n`, `1B 32` | `x 1B 33 48`, `x 1B 32` | Works (`ls n`, `lsd`) |
| **Bold on/off** | `1B 45 n` | `x 1B 45 01`, `x 1B 45 00` | Works |
| **Underline on/off** | `1B 2D n` | `x 1B 2D 01`, `x 1B 2D 00` | To test |
| **Align** | `1B 61 n` | `x 1B 61 00/01/02` | Works (`a L/C/R`) |
| **Font size (width×height)** | `1D 21 n` | `x 1D 21 00/10/01/11` | Works (`sz WxH`) |
| **Reverse print (black bg)** | `1D 42 n` | `x 1D 42 01` | To test |
| **Cut** | `1D 56 n` | `x 1D 56 00` | Likely unsupported (no cutter) |
| **Status request** | `10 04 n` | `x 10 04 01–05` | Works (1/2/4 only) |
| **Self-test** | `12 54` | `x 12 54` | Works |
| **Print & feed** | `1B 4A n` / `0A` | Verified via `jd n`, `LF` |
| **Image / bit-image** | `1B 2A m nL nH [data]` or `1D 76 30 m xL xH yL yH [data]` | (future bitmap tests) | TBD |
| **Barcode** | `1D 6B m [data] 00` | `x 1D 6B 04 34 35 36 00` | TBD (Code-39/128) |
| **Cash drawer pulse** | `1B 70 m t1 t2` | `x 1B 70 00 64 64` | Possibly no effect |



## 🧠 CSN-A1X Thermal Printer ↔ ESP32 Setup Summary

Original ChatGPT chat: https://chatgpt.com/share/68ea877c-d750-800a-bf36-cbff5c3e46a4

1️⃣ Power

Function	Connection	Details
Printer power	Bench PSU (or dedicated 5 V/3 A wall supply) → VH+	5 V DC, ≥ 2 A required (9 V OK if board rated).
Printer ground	Bench PSU GND → printer GND	Shared with ESP32.
ESP32 power	Via USB-C from computer or 5 V regulator	Independent from printer PSU but same GND.

All grounds must meet at one point (“star” ground):

ESP32 GND ─┬─ Printer GND (TTL pin 4)
            └─ Bench PSU GND


⸻

2️⃣ UART (logic) wiring — crossed pair

Printer TTL Pin	Signal	Connects To ESP32	Notes
1	DTRD	leave unconnected (optional +5 V pull-up 10 kΩ if needed later)	Flow-control; not used.
2	TXD	GPIO 19 (RX2)	Printer → ESP32 data.
3	RXD	GPIO 18 (TX2)	ESP32 → Printer data.
4	GND	ESP32 GND + Bench GND	Common reference.

✅ Confirmed idle voltage at printer RXD ≈ 3.3 V from ESP32 TX.
✅ Working baud = 9600 8N1.

⸻

3️⃣ Working test sketch

#include <HardwareSerial.h>
HardwareSerial P(2);

void setup() {
  Serial.begin(115200);

  // UART2: RX=19, TX=18  —  9600 baud 8N1
  P.begin(9600, SERIAL_8N1, 19, 18);
  delay(200);

  // Initialize printer (ESC @)
  P.write(0x1B); P.write('@');

  // Print message
  P.println("HELLO\n\nAria and Xavier!!\n\nGreetings from Jerry the Printer!\n\n");
  P.write('\n'); P.write('\n'); P.write('\n');

  Serial.println("Sent test at 9600.");
}

void loop() {}


⸻

4️⃣ Expected behavior
	•	Printer LED flashes once on receipt, then prints the text cleanly.
	•	Bench PSU shows transient current peaks (≈ 0.8–1.5 A).
	•	ESP32 serial monitor outputs Sent test at 9600.

⸻

5️⃣ Future expansion options
	•	Change baud via printer command if needed: ESC BAUD (or DIP if present).
	•	Status query (optional): send DLE EOT n (0x10 0x04 n, n = 1–5) and read replies.
	•	Flow control: tie DTRD high through 10 kΩ → +5 V if you later enable it.
	•	Logic-level buffer: if you redesign the board and run longer cables, insert a 74HCT14/125 for robust 5 V signaling.

⸻

✅ TL;DR — one-line summary

ESP32 TX (18) → Printer RXD (3), ESP32 RX (19) ← Printer TXD (2), All GNDs common, Printer 5 V ≥ 2 A, 9600 8N1.




## Bitmap Printing (Proof of Concept)

We verified the CSN-A1X prints raster images using the Epson `GS v 0` (“raster bit image”) command.  
Max printable width is **384 px** (~48 mm on 58 mm paper, 8 dots/mm). Height is unlimited.

**Pipeline summary**
1. Convert any PNG → 1-bit packed C header using Pillow:  
   ```bash
   python3 png2c.py <image.png> 384 <Symbol> --style=string
   # example:
   # python3 png2c.py pirate-crossbones.png 384 PirateLogo --style=string

	•	Produces <Symbol>.h with:

#define <Symbol>_W <width>
#define <Symbol>_H <height>
const uint8_t PROGMEM <Symbol>[] = "\xFF\xFF...";


	•	Pillow "1" mode uses 0 = black, 255 = white; our script sets bits where pixel == 0 (black).
	•	If polarity looks reversed, rerun with --invert or call printBitmap(..., invert=true).

	2.	Include and print in the sketch:

#include "PirateLogo.h"
escInit();
printBitmap(PirateLogo, PirateLogo_W, PirateLogo_H, /*center=*/true);
feedLF(3);



Notes
	•	Bitmaps must be ≤ 384 px wide; extra width is clipped.
	•	1 bit per pixel, MSB = leftmost, 1 = black.
	•	For transparent PNGs, flatten onto white before conversion.
	•	Default mode = 0 (normal density).
	•	Current draw during full-width graphics ≈ 1 A at 5 V.
	•	This method lets us embed logos or art directly in firmware until SD storage is added.







## Test Program (ino)
- interactive (send serial commands back via your serial monitor) and tests most major functions

// ESP32 ↔ CSN-A1X Thermal Printer CLI (9600 baud model)
// Pins: Printer TXD->GPIO19, Printer RXD<-GPIO18, GND common. Power printer from 5V≥2A.

#include <HardwareSerial.h>
HardwareSerial PRN(2);

static const int PIN_RX = 19;   // printer TXD -> ESP32 RX
static const int PIN_TX = 18;   // printer RXD <- ESP32 TX
static const long BAUDS[] = {9600,19200,38400,57600};
static int baudIdx = 0;

void openUart(long b){ PRN.flush(); PRN.end(); delay(80); PRN.begin(b, SERIAL_8N1, PIN_RX, PIN_TX); }
long curBaud(){ return BAUDS[baudIdx]; }

void escInit(){ PRN.write(0x1B); PRN.write('@'); }
void feedLF(uint8_t n){ while(n--) PRN.write('\n'); }
void feedDots(uint8_t n){ PRN.write(0x1B); PRN.write('J'); PRN.write(n); }         // ESC J n
void setAlign(char j){ uint8_t n=(j=='L')?0:(j=='C')?1:(j=='R')?2:0; PRN.write(0x1B); PRN.write('a'); PRN.write(n); }
void setSize(uint8_t w, uint8_t h){ uint8_t n=((w>1)?1:0)<<4 | ((h>1)?1:0); PRN.write(0x1D); PRN.write('!'); PRN.write(n); }
void setLineSpacing(uint8_t n){ PRN.write(0x1B); PRN.write('3'); PRN.write(n); }   // ESC 3 n (dots)
void setLineSpacingDefault(){ PRN.write(0x1B); PRN.write('2'); }                   // ESC 2 (default)

void selfTest(){ PRN.write(0x12); PRN.write('T'); }                                // DC2 'T'

// --- Status (DLE EOT n) with simple decoding relative to your observed baseline 0x12 ---
uint8_t eotRaw(uint8_t n){
  while(PRN.available()) PRN.read();                // clear
  PRN.write(0x10); PRN.write(0x04); PRN.write(n);   // DLE EOT n
  delay(60);
  if (PRN.available()) return (uint8_t)PRN.read();
  return 0xFF;                                      // no reply
}
void statusReport(){
  uint8_t s1=eotRaw(1), s2=eotRaw(2), s4=eotRaw(4), s5=eotRaw(5);
  auto pr=[&](const char* tag, uint8_t v){ if(v==0xFF) Serial.printf("%s -> (no reply)\n", tag);
                                           else        Serial.printf("%s -> 0x%02X\n", tag, v); };
  pr("EOT1",s1); pr("EOT2",s2); pr("EOT4",s4); pr("EOT5",s5);

  // Empirical decode for this unit:
  bool ok1 = (s1==0x12), ok2 = (s2==0x12), ok4 = (s4==0x12);
  bool doorLikelyOpen = (!ok1 || !ok2 || !ok4) && (s1!=0xFF && s2!=0xFF && s4!=0xFF);
  Serial.print("Decoded: ");
  if (doorLikelyOpen) Serial.print("[COVER/DOOR OPEN] ");
  if (s5==0xFF) Serial.print("[No paper sensor reply] ");
  if (ok1 && ok2 && ok4) Serial.print("[READY]");
  Serial.println();
}

void demo(){
  setAlign('C'); setSize(2,2);
  PRN.println("ESP32 Thermal");
  feedLF(1);
  setSize(1,1);
  PRN.println("----------------");
  setAlign('L');
  PRN.println("Normal");
  // Bold on/off
  PRN.write(0x1B); PRN.write('E'); PRN.write(1); PRN.println("Bold");
  PRN.write(0x1B); PRN.write('E'); PRN.write(0);
  // Line spacing sample
  setLineSpacing(48); PRN.println("Wide line spacing (48 dots)");
  setLineSpacingDefault();
  feedLF(2);
}

void help(){
  Serial.println("i=init  t <text>=print  lf N=feed N lines  jd N=feed N dots");
  Serial.println("a L|C|R=align  sz WxH (1|2)x(1|2)  ls N=set line spacing (dots)  lsd=default spacing");
  Serial.println("s=status  T=selftest  b=cycle ESP32 UART baud (printer stays at 9600)");
}

void setup(){
  Serial.begin(115200);
  PRN.begin(curBaud(), SERIAL_8N1, PIN_RX, PIN_TX);
  delay(150);
  Serial.printf("\nThermal CLI ready. UART=%ld\n", curBaud());
  help();
}

void loop(){
  // Drain unsolicited bytes
  while(PRN.available()){ int b=PRN.read(); Serial.printf("[PRN]0x%02X\n", b); }

  if (!Serial.available()) return;
  String ln = Serial.readStringUntil('\n'); ln.trim(); if(!ln.length()) return;

  if(ln=="h"){ help(); return; }
  if(ln=="i"){ escInit(); Serial.println("Init."); return; }
  if(ln=="s"){ statusReport(); return; }
  if(ln=="T"){ selfTest(); Serial.println("Self-test."); return; }
  if(ln=="b"){ baudIdx=(baudIdx+1)% (sizeof(BAUDS)/sizeof(BAUDS[0])); openUart(curBaud()); Serial.printf("UART=%ld\n", curBaud()); return; }
  if(ln.startsWith("t ")){ PRN.println(ln.substring(2)); Serial.println("Printed."); return; }
  if(ln.startsWith("lf ")){ int n=ln.substring(3).toInt(); if(n<0)n=0; if(n>255)n=255; feedLF((uint8_t)n); Serial.printf("Fed %d LF\n",n); return; }
  if(ln.startsWith("jd ")){ int n=ln.substring(3).toInt(); if(n<0)n=0; if(n>255)n=255; feedDots((uint8_t)n); Serial.printf("Fed %d dots\n",n); return; }
  if(ln.startsWith("a ")){ char c=toupper((unsigned char)ln.substring(2)[0]); setAlign(c); Serial.printf("Align=%c\n",c); return; }
  if(ln.startsWith("sz ")){ int x=ln.indexOf('x',3); uint8_t w=1,h=1; if(x>0){ w=(uint8_t)ln.substring(3,x).toInt(); h=(uint8_t)ln.substring(x+1).toInt(); }
                            setSize(w,h); Serial.printf("Size=%ux%u\n",w,h); return; }
  if(ln=="lsd"){ setLineSpacingDefault(); Serial.println("Line spacing: default"); return; }
  if(ln.startsWith("ls ")){ int n=ln.substring(3).toInt(); if(n<0)n=0; if(n>255)n=255; setLineSpacing((uint8_t)n); Serial.printf("Line spacing=%d dots\n",n); return; }

  Serial.println("Unknown. h=help.");
}