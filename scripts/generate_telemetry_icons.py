#!/usr/bin/env python3
"""Regenerate the attributed, checked-in Font Awesome telemetry masks."""
import io
import json
from pathlib import Path
import cairosvg
from PIL import Image

ROOT = Path(__file__).resolve().parents[1] / 'Services/scene_host/fonts'
manifest = json.loads((ROOT / 'TELEMETRY-ICONS.json').read_text())
lines = [
    '/* Font Awesome Free glyph masks from owner-supplied mockup; CC BY 4.0. */',
    'static const uint32_t tm_icons[][32]={'
]
for name in manifest['order']:
    glyph = manifest['icons'][name]
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {glyph["w"]} {glyph["h"]}">'
           f'<path d="{glyph["d"]}"/></svg>')
    image = Image.open(io.BytesIO(cairosvg.svg2png(
        bytestring=svg.encode(), output_width=32, output_height=32))).convert('RGBA')
    rows = [sum(1 << (31-x) for x in range(32) if image.getpixel((x, y))[3] >= 128)
            for y in range(32)]
    lines.append('{' + ','.join(f'0x{value:08x}u' for value in rows) + '}, /* ' + name + ' */')
lines.append('};')
(ROOT / 'telemetry_icons.inc').write_text('\n'.join(lines) + '\n')
