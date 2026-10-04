#!/usr/bin/env python3
"""Generate renamed RiscPortableSettingsText rasters from the NOVA OFL sources."""
import argparse,hashlib,json
from pathlib import Path
from PIL import ImageFont
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--fonts',required=True,type=Path);a=p.parse_args()
out=ROOT/'lib/PortableApps/settings_fonts';out.mkdir(exist_ok=True)
faces=[('Orbitron-700.ttf',14),('Rajdhani-600.ttf',15),('Rajdhani-600.ttf',12),('Rajdhani-600.ttf',11),('Orbitron-700.ttf',30),('Rajdhani-600.ttf',22)]
arrays=[];rows=[];record={'source':'Unmodified NOVA font inputs; raster subset renamed RiscPortableSettingsText','license':'SIL OFL 1.1','files':{},'faces':faces}
for face,(name,size) in enumerate(faces):
 path=a.fonts/name;record['files'][name]=hashlib.sha256(path.read_bytes()).hexdigest();font=ImageFont.truetype(str(path),size)
 for cp in range(32,127):
  # Large centered values need digits and hyphen only; no unused large alphabet.
  ch=chr(cp) if (face<4 or face==4 and chr(cp) in '0123456789-' or face==5 and chr(cp) in '+-') else ' '
  mask,offset=font.getmask2(ch,mode='L');w,h=mask.size;packed=[];value=0
  for i,pixel in enumerate(mask):
   value=(value<<2)|(pixel*3//255)
   if i%4==3:packed.append(value);value=0
  if w*h%4:packed.append(value<<(2*(4-w*h%4)))
  if not packed:packed=[0]
  ident=f'rps_{face}_{cp}';arrays.append('static const uint8_t '+ident+'[]={'+','.join(map(str,packed))+'};')
  rows.append('{%d,%d,%d,%d,%d,%s}'%(w,h,offset[0],offset[1],round(font.getlength(ch)*16),ident))
content='/* Generated RiscPortableSettingsText. Orbitron/Rajdhani SIL OFL1.1.\n * See SOURCES.json and the retained licenses. */\n'+'\n'.join(arrays)+'\ntypedef struct { uint8_t width,height; int8_t left,top; uint16_t advance_q4; const uint8_t *bits; } rps_glyph;\nstatic const rps_glyph rps_text[]={\n'+',\n'.join(rows)+'\n};\n'
(out/'text.inc').write_text(content)
record['text.inc_sha256']=hashlib.sha256(content.encode()).hexdigest()
for family in ['Orbitron','Rajdhani']:(out/('LICENSE-'+family+'.txt')).write_bytes((a.fonts/(family+'-OFL.txt')).read_bytes())
(out/'SOURCES.json').write_text(json.dumps(record,indent=2)+'\n')
