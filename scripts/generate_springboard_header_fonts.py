#!/usr/bin/env python3
"""Exact 30px Orbitron header subsets from the supplied NOVA-7 reference."""
from pathlib import Path
from PIL import ImageFont
import hashlib,json
ROOT=Path(__file__).resolve().parents[1]
sources={w:ROOT/f'lib/PortableApps/home_fonts/Orbitron-reference-{w}.ttf' for w in (700,900)}
out=ROOT/'lib/PortableApps/paper_fonts'
content=['/* Orbitron 30px, SIL OFL1.1. Generated from the existing licensed source. */','typedef struct {uint8_t code,width,height;int8_t left,top;uint16_t advance;const uint8_t *bits;} sbh_glyph;','typedef struct {const sbh_glyph *glyphs;unsigned count;} sbh_font;']
for weight,chars in [(900,'APS'),(700,'0123456789:-')]:
 f=ImageFont.truetype(str(sources[weight]),30);rows=[]
 for ch in chars:
  mask,xy=f.getmask2(ch,mode='L',anchor='ls');w,h=mask.size;bits=bytearray((w*h+7)//8 or 1)
  for i,p in enumerate(mask):
   if p>=128:bits[i//8]|=128>>(i%8)
  ident=f'sbh_{weight}_{ord(ch)}';content.append('static const uint8_t '+ident+'[]={'+','.join(map(str,bits))+'};');rows.append('{%d,%d,%d,%d,%d,%d,%s}'%(ord(ch),w,h,*xy,round(f.getlength(ch)*64),ident))
 content.append('static const sbh_glyph sbh_glyphs_'+str(weight)+'[]={'+','.join(rows)+'};');content.append('static const sbh_font SBH_'+str(weight)+'={sbh_glyphs_'+str(weight)+','+str(len(chars))+'};')
text='\n'.join(content)+'\n';(out/'springboard_header.inc').write_text(text)
(out/'SPRINGBOARD_SOURCES.json').write_text(json.dumps({'sources':{str(w):{'path':str(p.relative_to(ROOT)),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for w,p in sources.items()},'license':'SIL OFL 1.1','anchor':'left-baseline','size_px':30,'threshold':128,'weights':{'title':900,'clock':700},'title_tracking_px':4,'text_sha256':hashlib.sha256(text.encode()).hexdigest()},indent=2)+'\n')
