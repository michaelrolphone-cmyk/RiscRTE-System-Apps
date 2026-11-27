#!/usr/bin/env python3
"""Native-size Settings font banks from the retained, licensed original faces."""
import hashlib,json,math
from pathlib import Path
from PIL import ImageFont
R=Path(__file__).resolve().parents[1]
fonts=R/'lib/PortableApps/quick_fonts';out=R/'lib/PortableApps/settings_fonts'
faces=[('Orbitron-700.ttf',14),('Rajdhani-600.ttf',15),('Rajdhani-600.ttf',12),('Rajdhani-600.ttf',11),('Orbitron-700.ttf',30),('Rajdhani-600.ttf',22)]
sides=[300,330,360,400,480]
a=[];rows=[];sizes=[]
for bank,side in enumerate(sides):
 sizes.append([])
 for face,(name,size) in enumerate(faces):
  size=max(17,math.ceil(size*side/240));sizes[-1].append(size);font=ImageFont.truetype(str(fonts/name),size)
  for cp in range(32,127):
   ch=chr(cp) if face<4 or face==4 and chr(cp) in '0123456789-.:/' or face==5 and chr(cp) in '+-' else ' '
   mask,offset=font.getmask2(ch,mode='L');w,h=mask.size;bits=[];v=0
   for i,p in enumerate(mask):
    v=(v<<2)|(p*3//255)
    if i%4==3:bits.append(v);v=0
   if w*h%4:bits.append(v<<(2*(4-w*h%4)))
   ident=f'rps_native_{bank}_{face}_{cp}';a.append(f'static const uint8_t {ident}[]={{'+','.join(map(str,bits or [0]))+'};')
   rows.append('{%d,%d,%d,%d,%d,%s}'%(w,h,*offset,round(font.getlength(ch)*16),ident))
content='/* Generated native Settings Orbitron/Rajdhani rasters. Retained SIL OFL1.1 licenses apply. */\n'+'\n'.join(a)+'\nstatic const int rps_native_sides[]={'+','.join(map(str,sides))+'};\nstatic const rps_glyph rps_native_text[]={\n'+',\n'.join(rows)+'\n};\n'
(out/'native.inc').write_text(content)
(out/'NATIVE-SOURCES.json').write_text(json.dumps({'source_files':{n:hashlib.sha256((fonts/n).read_bytes()).hexdigest() for n,_ in faces},'layout_sides':sides,'native_sizes':sizes,'native.inc_sha256':hashlib.sha256(content.encode()).hexdigest(),'license':'SIL OFL 1.1; unchanged font inputs'},indent=2)+'\n')
