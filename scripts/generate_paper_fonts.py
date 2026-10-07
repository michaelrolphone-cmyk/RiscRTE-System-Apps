#!/usr/bin/env python3
"""Generate auditable one-bit Nova7 text from the existing licensed font inputs."""
import hashlib,json,pathlib
from PIL import ImageFont
ROOT=pathlib.Path(__file__).resolve().parents[1]
source=ROOT/'lib/PortableApps/quick_fonts'
out=ROOT/'lib/PortableApps/paper_fonts';out.mkdir(exist_ok=True)
arrays=[];rows=[];inputs={}
for family,size in [('Orbitron-700.ttf',28),('Rajdhani-600.ttf',20)]:
    path=source/family;inputs[str(path.relative_to(ROOT))]=hashlib.sha256(path.read_bytes()).hexdigest()
    font=ImageFont.truetype(str(path),size)
    for cp in range(32,127):
        mask,offset=font.getmask2(chr(cp),mode='L');w,h=mask.size;packed=bytearray((w*h+7)//8 or 1)
        for i,pixel in enumerate(mask):
            if pixel>=128:packed[i//8]|=0x80>>(i%8)
        ident=f'rpp_{size}_{cp}'
        arrays.append('static const uint8_t '+ident+'[] = {'+','.join(map(str,packed))+'};')
        rows.append('{%d,%d,%d,%d,%d,%s}'%(w,h,*offset,round(font.getlength(chr(cp))*16),ident))
content='/* RiscPaperText: 1-bit Orbitron 700 / Rajdhani 600 raster subsets.\n * SIL OFL 1.1. See SOURCES.json and adjacent licenses. */\n'+'\n'.join(arrays)+'\ntypedef struct { uint8_t width,height; int8_t left,top; uint16_t advance_q4; const uint8_t *bits; } rpp_glyph;\nstatic const rpp_glyph rpp_text[] = {\n'+',\n'.join(rows)+'\n};\n'
(out/'text.inc').write_text(content)
for family in ['Orbitron','Rajdhani']:(out/f'LICENSE-{family}.txt').write_bytes((source/f'{family}-OFL.txt').read_bytes())
(out/'SOURCES.json').write_text(json.dumps({'license':'SIL OFL 1.1','inputs_sha256':inputs,'text.inc_sha256':hashlib.sha256(content.encode()).hexdigest(),'sizes':{'Orbitron-700':28,'Rajdhani-600':20},'threshold':128},indent=2)+'\n')
