#!/usr/bin/env python3
"""Reproduce the ten 28px FA masks from the supplied NOVA Reader HTML.

Requires cairosvg and Pillow. The input is a local design reference; this
script makes no network requests and does not import or redistribute fonts.
"""
import argparse
import hashlib
import io
import json
import re
from pathlib import Path
import cairosvg
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
NAMES = ['book', 'bookmark', 'bmo', 'font', 'sliders', 'plus', 'chevL', 'list', 'trash', 'x']
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('reference', type=Path)
p.add_argument('--output', type=Path, default=ROOT/'Services/scene_host/fonts/document_icons.inc')
a = p.parse_args()
text = a.reference.read_text()
match = re.search(r'const ICN=(\{.*?\});', text)
if not match:
    raise ValueError('The reference contains no ICN object')
icons = json.loads(match.group(1))
lines = ['/* Font Awesome Free by Fonticons, Inc. CC BY 4.0.',
         ' * Paths supplied in Reader (eBooks) - NOVA-7 watch + e-ink.html.',
         ' * https://fontawesome.com/license/free - icons only, no font asset.',
         ' * Regenerate: python scripts/reader_reference_icons.py reference.html',
         ' */', 'static const uint32_t nc_doc_icons[10][28]={']
for name in NAMES:
    icon = icons[name]
    svg = f'<svg xmlns="http://www.w3.org/2000/svg" width="28" height="28" viewBox="0 0 {icon["w"]} {icon["h"]}"><path fill="black" d="{icon["d"]}"/></svg>'
    image = Image.open(io.BytesIO(cairosvg.svg2png(bytestring=svg.encode(), output_width=28, output_height=28))).convert('RGBA')
    rows = [sum(1 << (27-x) for x in range(28) if image.getpixel((x,y))[3] >= 128) for y in range(28)]
    lines.append(' {' + ','.join(f'0x{v:07x}u' for v in rows) + '}, /* ' + name + ' */')
lines.append('};')
a.output.write_text('\n'.join(lines)+'\n')
print('Reference SHA256:', hashlib.sha256(a.reference.read_bytes()).hexdigest())
