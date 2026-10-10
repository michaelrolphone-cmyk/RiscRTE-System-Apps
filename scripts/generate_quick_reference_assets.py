#!/usr/bin/env python3
"""Rasterize host-only Quick Actions assets from the supplied reference."""
from pathlib import Path
import copy,hashlib,json,os,shutil,subprocess,tempfile,xml.etree.ElementTree as ET
from PIL import Image,ImageFont
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'lib/PortableApps/quick_fonts'
FONTS=ROOT/'lib/PortableApps/home_fonts'
SOURCE=ROOT/'test/native_apps/fixtures/quick-refined-reference.svg'
faces=[('TITLE','Orbitron-reference-900.ttf',24,'QUICK ACTIONS'),
 ('TIME','Orbitron-reference-700.ttf',26,'0123456789:-'),
 ('NUMBER','Orbitron-reference-700.ttf',24,'0123456789%+- '),
 ('LABEL','Rajdhani-700.ttf',17,'FRONTLIGHT VOLUME'),
 ('TILE','Rajdhani-700.ttf',20,None),('SMALL','Rajdhani-600.ttf',16,None),
 ('BATTERY','Rajdhani-700.ttf',20,'BATT')]
lines=['/* Generated host-only reference assets. See REFERENCE.json. */']
manifest={'generator':'scripts/generate_quick_reference_assets.py','license':'Fonts SIL OFL 1.1; vectors supplied in the user reference','reference_svg_sha256':hashlib.sha256(SOURCE.read_bytes()).hexdigest(),'fonts':[]}
for ident,filename,size,chars in faces:
 font=ImageFont.truetype(str(FONTS/filename),size);chars=set(chars or ''.join(chr(c) for c in range(32,127) if not 97<=c<=122));rows=[];kern=[]
 for cp in range(32,127):
  if chr(cp) not in chars:rows.append('{0,0,0,0,0,0}');continue
  mask,offset=font.getmask2(chr(cp),mode='L',anchor='ls');w,h=mask.size;bits=bytearray((w*h+7)//8 or 1)
  for i,value in enumerate(mask):
   if value>=128:bits[i//8]|=128>>(i%8)
  name=f'quick_ref_{ident}_{cp}';lines.append('static const uint8_t '+name+'[]={'+','.join(map(str,bits))+'};')
  rows.append('{%d,%d,%d,%d,%d,%s}'%(w,h,*offset,round(font.getlength(chr(cp))*64),name))
 for a in sorted(chars):
  for b in sorted(chars):
   k=round((font.getlength(a+b)-font.getlength(a)-font.getlength(b))*64)
   if k:kern.append('{%d,%d,%d}'%(ord(a),ord(b),k))
 lines+=['static const quick_ref_glyph quick_ref_glyphs_'+ident+'[]={'+','.join(rows)+'};','static const quick_ref_kern quick_ref_kerns_'+ident+'[]={'+(','.join(kern) if kern else '{0,0,0}')+'};','static const quick_ref_font QUICK_REF_'+ident+'={quick_ref_glyphs_'+ident+',quick_ref_kerns_'+ident+','+str(len(kern))+'};']
 manifest['fonts'].append({'id':ident,'file':'../home_fonts/'+filename,'size':size,'sha256':hashlib.sha256((FONTS/filename).read_bytes()).hexdigest()})
root=ET.parse(SOURCE).getroot();groups=root.findall('{http://www.w3.org/2000/svg}g')
manifest['icons']={}
with tempfile.TemporaryDirectory() as temporary:
 for name,index in [('silent',2),('dnd',3),('airplane',4),('wifi',5),('bluetooth',6),('frontlight',7),('sun',0),('volume',1),('clean',9)]:
  group=copy.deepcopy(groups[index]);group.set('transform','translate(0.8,0.8) scale(1.6)');group.set('stroke','#000')
  svg='<svg xmlns="http://www.w3.org/2000/svg" width="40" height="40" viewBox="0 0 40 40"><rect width="40" height="40" fill="#fff"/>'+ET.tostring(group,encoding='unicode')+'</svg>'
  path=OUT/('reference-'+name+'.svg');path.write_text(svg+'\n');png=Path(temporary)/(name+'.png')
  subprocess.run(['inkscape',str(path),'--export-type=png','--export-filename='+str(png)],check=True,capture_output=True)
  im=Image.open(png).convert('L');assert im.size==(40,40);bits=bytearray(200)
  for n,value in enumerate(im.getdata()):
   if value<128:bits[n//8]|=128>>(n%8)
  lines.append('static const uint8_t quick_ref_icon_'+name+'[]={'+','.join(map(str,bits))+'};');manifest['icons'][path.name]=hashlib.sha256(path.read_bytes()).hexdigest()
# Whole reference labels preserve the SVG renderer's shaping and textLength.
# Dynamic time/percent text retains the real font glyph and kerning tables.
with tempfile.TemporaryDirectory() as temporary:
 folder=Path(temporary);fonts=folder/'fonts';fonts.mkdir();cache=folder/'cache';cache.mkdir()
 for filename in {face[1] for face in faces}:shutil.copyfile(FONTS/filename,fonts/filename)
 config=folder/'fonts.conf';config.write_text('<fontconfig><dir>'+str(fonts)+'</dir><cachedir>'+str(cache)+'</cachedir></fontconfig>')
 env=dict(os.environ,FONTCONFIG_FILE=str(config));words=[]
 texts=root.findall('{http://www.w3.org/2000/svg}text')
 for index,ident,tracking,target in [(0,'TITLE',128,260),(2,'LABEL',256,0),(4,'LABEL',256,0),
   (6,'TILE',32,0),(7,'TILE',32,0),(8,'TILE',32,0),(9,'TILE',32,0),(10,'TILE',32,0),(11,'TILE',32,0),(13,'TILE',32,0),(14,'TILE',32,0),(15,'BATTERY',256,0)]:
  node=copy.deepcopy(texts[index]);node.set('x','32');node.set('y','60');node.set('fill','#000');node.set('text-anchor','start')
  svg='<svg xmlns="http://www.w3.org/2000/svg" width="480" height="90"><style>.o{font-family:Orbitron}.r{font-family:Rajdhani}</style><rect width="480" height="90" fill="#fff"/>'+ET.tostring(node,encoding='unicode')+'</svg>'
  path=folder/'word.svg';path.write_text(svg);png=folder/'word.png';subprocess.run(['inkscape',str(path),'--export-type=png','--export-filename='+str(png)],check=True,capture_output=True,env=env)
  im=Image.open(png).convert('L').point(lambda x:255 if x<128 else 0);box=im.getbbox();assert box;im=im.crop(box);w,h=im.size;bits=bytearray((w*h+7)//8)
  for n,value in enumerate(im.getdata()):
   if value:bits[n//8]|=128>>(n%8)
  name='quick_ref_word_'+str(index);lines.append('static const uint8_t '+name+'[]={'+','.join(map(str,bits))+'};')
  words.append('{&QUICK_REF_%s,%s,%d,%d,%d,%d,%d,%d,%s}'%(ident,json.dumps(node.text),tracking,target,w,h,box[0]-32,box[1]-60,name))
 lines.append('static const quick_ref_word quick_ref_words[]={'+','.join(words)+'};')
lines.append('static const uint8_t *const quick_ref_icons[]={quick_ref_icon_silent,quick_ref_icon_dnd,quick_ref_icon_airplane,quick_ref_icon_wifi,quick_ref_icon_bluetooth,quick_ref_icon_frontlight,quick_ref_icon_sun,quick_ref_icon_volume,quick_ref_icon_clean};')
text='\n'.join(lines)+'\n';(OUT/'reference.inc').write_text(text);manifest['reference.inc_sha256']=hashlib.sha256(text.encode()).hexdigest();(OUT/'REFERENCE.json').write_text(json.dumps(manifest,indent=2)+'\n')
