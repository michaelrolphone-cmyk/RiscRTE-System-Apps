#!/usr/bin/env python3
"""Rasterize the supplied Home SVG's fonts and dock vectors at native size."""
from pathlib import Path
import hashlib,io,json,subprocess,tempfile
from PIL import Image,ImageFont
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'lib/PortableApps/home_fonts'
faces=[('TIME','Orbitron-reference-900.ttf',142,'0123456789:-'),
 ('TITLE','Orbitron-reference-900.ttf',56,None),('EVENT','Orbitron-reference-700.ttf',26,'0123456789:AMP -'),
 ('BATTERY','Orbitron-reference-700.ttf',18,'0123456789%+- '),
 ('NEXT','Rajdhani-700.ttf',20,None),('THEN','Rajdhani-700.ttf',16,None),
 ('DOCK','Rajdhani-700.ttf',18,None),('FOLLOW','Rajdhani-700.ttf',21,None),
 ('DURATION','Rajdhani-600.ttf',19,None)]
lines=['/* Native assets from the supplied refined Home reference. See REFERENCE.json. */']
manifest={'generator':'scripts/generate_home_reference_assets.py','license':'Fonts SIL OFL 1.1; dock vectors supplied in the user reference','fonts':[]}
for ident,filename,size,chars in faces:
 font=ImageFont.truetype(str(OUT/filename),size)
 chars=set(chars or ''.join(chr(c) for c in range(32,127) if not 97<=c<=122))
 rows=[];kern=[]
 for cp in range(32,127):
  if chr(cp) not in chars:rows.append('{0,0,0,0,0,0}');continue
  mask,offset=font.getmask2(chr(cp),mode='L',anchor='ls');w,h=mask.size;bits=bytearray((w*h+7)//8 or 1)
  for i,value in enumerate(mask):
   if value>=128:bits[i//8]|=128>>(i%8)
  name=f'home_ref_{ident}_{cp}';lines.append('static const uint8_t '+name+'[]={'+','.join(map(str,bits))+'};')
  rows.append('{%d,%d,%d,%d,%d,%s}'%(w,h,*offset,round(font.getlength(chr(cp))*64),name))
 for a in sorted(chars):
  for b in sorted(chars):
   k=round((font.getlength(a+b)-font.getlength(a)-font.getlength(b))*64)
   if k:kern.append('{%d,%d,%d}'%(ord(a),ord(b),k))
 lines += ['static const home_glyph home_ref_glyphs_'+ident+'[]={'+','.join(rows)+'};',
  'static const home_kern home_ref_kerns_'+ident+'[]={'+(','.join(kern) if kern else '{0,0,0}')+'};',
  'static const home_font HOME_REF_'+ident+'={home_ref_glyphs_'+ident+',home_ref_kerns_'+ident+','+str(len(kern))+'};']
 manifest['fonts'].append({'id':ident,'file':filename,'size':size,'sha256':hashlib.sha256((OUT/filename).read_bytes()).hexdigest()})
with tempfile.TemporaryDirectory() as temporary:
 for i,name in enumerate(('files','points','contexts','settings')):
  svg=OUT/('reference-'+name+'.svg');png=Path(temporary)/(name+'.png')
  subprocess.run(['inkscape',str(svg),'--export-type=png','--export-filename='+str(png)],check=True,capture_output=True)
  im=Image.open(png).convert('L');assert im.size==(80,80);bits=bytearray(800)
  for n,value in enumerate(im.getdata()):
   if value<128:bits[n//8]|=128>>(n%8)
  lines.append('static const uint8_t home_ref_icon_'+name+'[]={'+','.join(map(str,bits))+'};')
 lines.append('static const uint8_t *const home_ref_icons[]={home_ref_icon_files,home_ref_icon_points,home_ref_icon_contexts,home_ref_icon_settings};')
text='\n'.join(lines)+'\n';(OUT/'reference.inc').write_text(text)
manifest['reference.inc_sha256']=hashlib.sha256(text.encode()).hexdigest()
manifest['source_html_sha256']='5f796971fc4914df2658a0241894d4c43c2dd8b2d0e0dd975b7b4f3f06ccad86'
manifest['source']='refined-home-design-library-read.html / Home, 2026-10-09'
manifest['reference_svg_sha256']=hashlib.sha256((ROOT/'test/native_apps/fixtures/home-refined-reference.svg').read_bytes()).hexdigest()
manifest['icons']={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in OUT.glob('reference-*.svg')}
(OUT/'REFERENCE.json').write_text(json.dumps(manifest,indent=2)+'\n')
