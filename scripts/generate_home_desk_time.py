#!/usr/bin/env python3
"""Generate the selected landscape NOVA time at its native 80-pixel size."""
from pathlib import Path
import hashlib,json
from PIL import ImageFont
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'lib/PortableApps/home_fonts'
source=out/'Orbitron-wght.ttf'
assert hashlib.sha256(source.read_bytes()).hexdigest()=='f42db2dd16e642258e35782916eceb1dcdbea06fb958d77ad71dc5963587e8fd'
font=ImageFont.truetype(str(source),80);font.set_variation_by_axes([900])
lines=['/* Native-size Orbitron900, 80px. SIL OFL1.1; see DESK_TIME.json. */'];rows=[]
for cp in range(32,127):
 ch=chr(cp)
 if ch not in '0123456789:-':rows.append('{0,0,0,0,0,0}');continue
 mask,offset=font.getmask2(ch,mode='L',anchor='ls');w,h=mask.size;bits=bytearray((w*h+7)//8 or 1)
 for i,value in enumerate(mask):
  if value>=128:bits[i//8]|=0x80>>(i%8)
 name='home_desk_time_'+str(cp)
 lines.append('static const uint8_t '+name+'[]={'+','.join(map(str,bits))+'};')
 rows.append('{%d,%d,%d,%d,%d,%s}'%(w,h,*offset,round(font.getlength(ch)*64),name))
lines.extend(['static const home_glyph home_desk_time_glyphs[]={'+','.join(rows)+'};',
 'static const home_font HOME_DESK_TIME={home_desk_time_glyphs,NULL,0};'])
text='\n'.join(lines)+'\n';(out/'desk_time.inc').write_text(text)
(out/'DESK_TIME.json').write_text(json.dumps({'font':'Orbitron-wght.ttf','weight':900,'size':80,'threshold':128,'anchor':'left-baseline','license':'SIL OFL1.1','source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'desk_time.inc_sha256':hashlib.sha256(text.encode()).hexdigest(),'generator':'scripts/generate_home_desk_time.py'},indent=2)+'\n')
