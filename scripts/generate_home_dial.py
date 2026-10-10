#!/usr/bin/env python3
"""One-bit butt-cap dial marks at the interactive SVG's physical geometry."""
from pathlib import Path
from PIL import Image,ImageDraw
import math,json,hashlib
ROOT=Path(__file__).resolve().parents[1];out=ROOT/'lib/PortableApps/home_fonts'
lines=['/* RiscHomeDial: original NOVA-7 interactive SVG tick geometry. */','typedef struct { int16_t x,y;uint8_t width,height;const uint8_t *bits; } home_tick;'];rows=[]
for i in range(60):
 angle=i*math.pi/30;dx,dy=math.sin(angle),-math.cos(angle);inner=168 if i%5==0 else 184
 for elapsed in (False,True):
  weight=9 if i%5==0 else 6 if elapsed else 2.5
  coords=[]
  for radius,side in [(inner,-1),(198,-1),(198,1),(inner,1)]:
   coords.append((240+dx*radius-dy*weight/2*side,256+dy*radius+dx*weight/2*side))
  left=math.floor(min(x for x,y in coords))-1;top=math.floor(min(y for x,y in coords))-1
  width=math.ceil(max(x for x,y in coords))-left+1;height=math.ceil(max(y for x,y in coords))-top+1
  ss=8;image=Image.new('L',(width*ss,height*ss));ImageDraw.Draw(image).polygon([((x-left)*ss,(y-top)*ss) for x,y in coords],fill=255)
  image=image.resize((width,height),Image.Resampling.LANCZOS);packed=bytearray((width*height+7)//8)
  for j,value in enumerate(image.getdata()):
   if value>=128:packed[j//8]|=128>>(j%8)
  name=f'home_tick_{i}_{int(elapsed)}';lines.append('static const uint8_t '+name+'[]={'+','.join(map(str,packed))+'};')
  rows.append('{%d,%d,%d,%d,%s}'%(left,top,width,height,name))
lines.append('static const home_tick home_ticks[]={'+','.join(rows)+'};')
text='\n'.join(lines)+'\n';(out/'dial.inc').write_text(text)
(out/'DIAL.json').write_text(json.dumps({'source':'NOVA-7 E-Ink Interactive — 480×800.html vFace','center':[240,256],'outer_radius':198,'inner_radius':{'major':168,'minor':184},'widths':{'major':9,'elapsed':6,'upcoming':2.5},'line_cap':'butt','supersample':8,'threshold':128,'dial.inc_sha256':hashlib.sha256(text.encode()).hexdigest()},indent=2)+'\n')
