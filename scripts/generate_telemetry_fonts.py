#!/usr/bin/env python3
"""Rasterize typography already used by NOVA; never copy browser simulation."""
from pathlib import Path
from PIL import ImageFont
import hashlib,json,argparse
R=Path(__file__).resolve().parents[1];F=R/'Services/scene_host/fonts'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,default=F/'telemetry.inc')
parser.add_argument('--metadata-output',type=Path,default=F/'TELEMETRY-SOURCES.json')
args=parser.parse_args()
roles=[(12,'Orbitron-900.ttf'),(7,'Orbitron-700.ttf'),(6.5,'Orbitron-700.ttf'),(52,'Orbitron-900.ttf'),(17,'Orbitron-900.ttf'),(8,'Rajdhani-700.ttf'),(40,'Orbitron-900.ttf'),(13,'Orbitron-900.ttf'),(11,'Rajdhani-700.ttf'),(11,'Orbitron-900.ttf')]
lines=['/* NOVA telemetry glyphs; generated; SIL OFL-1.1. */'];meta=[]
for scale in (1,2):
 for role,(sz,file) in enumerate(roles):
  font=ImageFont.truetype(str(F/file),round(sz*scale));name=f'tmfont_{scale}_{role}';g=[]
  for cp in range(32,127):
   text=chr(cp) if role not in (3,6) or chr(cp) in '0123456789.-' else ' '
   mask,off=font.getmask2(text,mode='L',anchor='ls');w,h=mask.size;buf=bytearray(max(1,(w*h+7)//8))
   for i,v in enumerate(mask):
    if v>=128:buf[i//8]|=128>>(i%8)
   sym=f'{name}_{cp}';lines.append('static const uint8_t '+sym+'[]={'+','.join(map(str,buf))+'};')
   g.append('{%d,%d,%d,%d,%d,%s}'%(w,h,*off,round(font.getlength(text)*64),sym))
  lines+=['static const pc_glyph '+name+'_glyphs[]={'+','.join(g)+'};','static const pc_font '+name+'={'+name+'_glyphs,NULL,0};']
  meta.append(dict(role=role,size=round(sz*scale),source=file,sha256=hashlib.sha256((F/file).read_bytes()).hexdigest()))
args.output.write_text('\n'.join(lines)+'\n');args.metadata_output.write_text(json.dumps(meta,indent=2)+'\n')
