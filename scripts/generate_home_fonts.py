#!/usr/bin/env python3
"""Native-size, baseline-anchored Home typography from licensed Google fonts."""
from pathlib import Path
from PIL import ImageFont
import hashlib,json
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'lib/PortableApps/home_fonts'
faces=[('R700_22','Rajdhani-700.ttf',22,700),('O700_24','Orbitron-wght.ttf',24,700),
 ('O900_52','Orbitron-wght.ttf',52,900),('R700_30','Rajdhani-700.ttf',30,700),
 ('O700_28','Orbitron-wght.ttf',28,700),('R700_28','Rajdhani-700.ttf',28,700),
 ('R600_22','Rajdhani-600.ttf',22,600),('O700_20','Orbitron-wght.ttf',20,700),
 ('O900_100','Orbitron-wght.ttf',100,900),('O600_18','Orbitron-wght.ttf',18,600),
 ('R700_26','Rajdhani-700.ttf',26,700)]
content=['/* RiscHomeText: native-size one-bit raster subsets. SIL OFL 1.1.\n * Original font inputs and weights are recorded in SOURCES.json. */',
 'typedef struct { uint8_t width,height; int8_t left,top; uint16_t advance_q6; const uint8_t *bits; } home_glyph;',
 'typedef struct { uint8_t first,second; int16_t adjust_q6; } home_kern;',
 'typedef struct { const home_glyph *glyphs; const home_kern *kern; unsigned kern_count; } home_font;']
manifest={'description':'Native physical size, no magnification of smaller bitmaps','license':'SIL OFL 1.1','threshold':128,'anchor':'left-baseline','fonts':[]}
charsets={'O900_100':'0123456789:-','O700_24':'AT0123456789:MP -','O700_28':'0123456789:AMP -','O700_20':'0123456789%-+ ','O600_18':'NOVA-7'}
for ident,file,size,weight in faces:
 charset=set(charsets.get(ident,''.join(chr(c) for c in range(32,127) if not 97<=c<=122)))
 font=ImageFont.truetype(str(out/file),size)
 if file=='Orbitron-wght.ttf':font.set_variation_by_axes([weight])
 rows=[];kerning=[]
 for cp in range(32,127):
  ch=chr(cp)
  if ch not in charset:
   rows.append('{0,0,0,0,0,0}');continue
  mask,offset=font.getmask2(ch,mode='L',anchor='ls');w,h=mask.size;packed=bytearray((w*h+7)//8 or 1)
  for i,p in enumerate(mask):
   if p>=128:packed[i//8]|=0x80>>(i%8)
  name=f'home_{ident}_{cp}'
  content.append('static const uint8_t '+name+'[]={'+','.join(map(str,packed))+'};')
  rows.append('{%d,%d,%d,%d,%d,%s}'%(w,h,*offset,round(font.getlength(ch)*64),name))
 for a in sorted(map(ord,charset)):
  for b in sorted(map(ord,charset)):
   k=round((font.getlength(chr(a)+chr(b))-font.getlength(chr(a))-font.getlength(chr(b)))*64)
   if k:kerning.append('{%d,%d,%d}'%(a,b,k))
 content.append('static const home_glyph home_glyphs_'+ident+'[]={'+','.join(rows)+'};')
 content.append('static const home_kern home_kerns_'+ident+'[]={'+(','.join(kerning) if kerning else '{0,0,0}')+'};')
 content.append('static const home_font HOME_'+ident+'={home_glyphs_'+ident+',home_kerns_'+ident+','+str(len(kerning))+'};')
 manifest['fonts'].append({'id':ident,'source':file,'size':size,'weight':weight,'sha256':hashlib.sha256((out/file).read_bytes()).hexdigest(),'kerning_pairs':len(kerning),'characters':''.join(sorted(charset))})
text='\n'.join(content)+'\n';(out/'text.inc').write_text(text);manifest['text.inc_sha256']=hashlib.sha256(text.encode()).hexdigest()
manifest['source_records']=json.loads((out/'downloads.json').read_text());(out/'SOURCES.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Generated Home raster fonts:',len(text),'source bytes;',[(r['id'],r['kerning_pairs']) for r in manifest['fonts']])
