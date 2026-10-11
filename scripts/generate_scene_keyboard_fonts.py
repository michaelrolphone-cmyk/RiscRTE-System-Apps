#!/usr/bin/env python3
"""Rasterize exact licensed fonts for the supplied 480x800 NOVA-7 reference."""
import hashlib,json
from fontTools.ttLib import TTFont
from pathlib import Path
from PIL import ImageFont
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'Services/scene_host/fonts'
source=out
faces=[('O900_26',900,26,'Orbitron-900.ttf'),('O900_30',900,30,'Orbitron-900.ttf'),('O900_38',900,38,'Orbitron-900.ttf'),('O900_46',900,46,'Orbitron-900.ttf'),('O700_28',700,28,'Orbitron-700.ttf'),('O700_22',700,22,'Orbitron-700.ttf'),('O700_20',700,20,'Orbitron-700.ttf')]
faces += [(f'R700_{size}',700,size,'Rajdhani-700.ttf') for size in (16,17,18,20)]
expected={name:hashlib.sha256((source/name).read_bytes()).hexdigest() for _,_,_,name in faces}
content=['/* Generated from licensed Orbitron and Rajdhani. See SOURCES.json. */','typedef struct {uint8_t width,height;int8_t left,top;uint16_t advance;const uint8_t *bits;} pc_glyph;','typedef struct {uint8_t first,second;int16_t adjust;} pc_kern;','typedef struct {const pc_glyph *glyphs;const pc_kern *kern;unsigned kern_count;} pc_font;']
manifest={'source':'Recovered user NOVA-7 HTML font assets; static Orbitron 700/900 and Rajdhani 700','license':'SIL OFL1.1','anchor':'left-baseline','threshold':128,'fonts':[]}
for ident,weight,size,name in faces:
 charset='0123456789:' if size>=52 else ''.join(chr(c) for c in range(32,127))
 font=ImageFont.truetype(str(source/name),size)
 
 cmap=TTFont(source/name).getBestCmap()
 fallback=ImageFont.truetype(str(source/'Rajdhani-700.ttf'),size)
 rows=[];kern=[]
 for cp in range(32,127):
  if chr(cp) not in charset:rows.append('{0,0,0,0,0,NULL}');continue
  face=font if cp in cmap else fallback
  mask,offset=face.getmask2(chr(cp),mode='L',anchor='ls');w,h=mask.size;bits=bytearray((w*h+7)//8 or 1)
  for i,value in enumerate(mask):
   if value>=128:bits[i//8]|=128>>(i%8)
  symbol=f'pc_font_{ident}_{cp}';content.append('static const uint8_t '+symbol+'[]={'+','.join(map(str,bits))+'};')
  rows.append('{%d,%d,%d,%d,%d,%s}'%(w,h,*offset,round(face.getlength(chr(cp))*64),symbol))
 for first in charset:
  for second in charset:
   value=round((font.getlength(first+second)-font.getlength(first)-font.getlength(second))*64)
   if value:kern.append('{%d,%d,%d}'%(ord(first),ord(second),value))
 content.append('static const pc_glyph pc_glyph_'+ident+'[]={'+','.join(rows)+'};')
 content.append('static const pc_kern pc_kern_'+ident+'[]={'+(','.join(kern) if kern else '{0,0,0}')+'};')
 content.append('static const pc_font PCF_'+ident+'={pc_glyph_'+ident+',pc_kern_'+ident+','+str(len(kern))+'};')
 manifest['fonts'].append({'id':ident,'file':name,'size':size,'weight':weight,'sha256':expected[name],'characters':charset,'fallback_characters':''.join(c for c in charset if ord(c) not in cmap),'fallback':'Rajdhani-700.ttf'})
text='\n'.join(content)+'\n';(out/'text.inc').write_text(text);manifest['text_sha256']=hashlib.sha256(text.encode()).hexdigest();(out/'SOURCES.json').write_text(json.dumps(manifest,indent=2)+'\n')

print('Shared NOVA-7 keyboard glyphs:',len(text),'source bytes')
