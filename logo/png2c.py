# png2c.py  —  PNG -> 1-bit packed C header for ESC/POS
# Usage:
#   python3 png2c.py <in.png> <max_width> <Symbol> [--style=string]
# - Emits packed MSB-first, 1=black
# - --style=string   => compact "\xHH..." bytes in a single literal
# exaple: python3 png2c.py pirate-crossbones.png 384 PirateLogo --style=string

from PIL import Image
import sys, os

def help_exit():
    print("Usage: python3 png2c.py <in.png> <max_width> <Symbol> [--style=string]")
    sys.exit(1)

if len(sys.argv) < 4: help_exit()
path, maxw, sym = sys.argv[1], int(sys.argv[2]), sys.argv[3]
style_string = ("--style=string" in sys.argv)

im = Image.open(path).convert("L")
if im.width > maxw:
    h = int(im.height * (maxw / im.width))
    im = im.resize((maxw, h), Image.LANCZOS)
im = im.convert("1")  # 1-bit FS dither

w, h = im.size
row_bytes = (w + 7) // 8
px = im.load()

buf = bytearray(row_bytes * h)
for y in range(h):
    for x in range(w):
        if px[x, y] == 0:  # black
            i = y * row_bytes + (x // 8)
            buf[i] |= (1 << (7 - (x % 8)))

def as_hex_list(b):
    parts = []
    for i, v in enumerate(b, 1):
        parts.append("0x%02X" % v)
        if i % 12 == 0: parts.append("\n  ")
    return ", ".join(parts)

def as_hex_string(b):
    # Chunk for readability; the compiler concatenates adjacent string literals
    CHUNK = 32
    lines = []
    for i in range(0, len(b), CHUNK):
        chunk = b[i:i+CHUNK]
        s = "".join("\\x%02X" % v for v in chunk)
        lines.append('  "' + s + '"')
    return "\n".join(lines)

if style_string:
    body = as_hex_string(buf)
    init = f'const uint8_t PROGMEM {sym}[] = \n{body};\n'
else:
    body = as_hex_list(buf)
    init = f'const uint8_t PROGMEM {sym}[] = {{\n  {body}\n}};\n'

hdr = (
    "#pragma once\n"
    "#include <stdint.h>\n"
    "#include <pgmspace.h>\n"
    f"#define {sym}_W {w}\n"
    f"#define {sym}_H {h}\n"
    f"{init}"
)

out_name = sym + ".h"
with open(out_name, "w") as f:
    f.write(hdr)

print(f"Wrote {out_name} ({w}x{h}, {row_bytes} bytes/row)  style={'string' if style_string else 'array'}")