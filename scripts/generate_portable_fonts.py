#!/usr/bin/env python3
"""Extract genuine FAClassic glyphs; never synthesize a substitute icon.
Inputs are an audited Reader checkout and the unmodified NOVA TTF inputs.
The generated rasters are renamed RiscPortableIcons/RiscPortableText (OFL).
"""
import argparse, csv, hashlib, json, pathlib, struct
from PIL import ImageFont
ROOT=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--reader',type=pathlib.Path,required=True);p.add_argument('--fonts',type=pathlib.Path,required=True);a=p.parse_args()
out=ROOT/'lib/PortableApps/fonts';out.mkdir(exist_ok=True)
# The deployed cross-repository inventory is an explicit shared contract.
registry=json.loads((ROOT/'lib/PortableApps/catalog-icons.json').read_text())['apps']
assert len({v['icon'] for v in registry.values()})==len(registry), 'Every app needs its own icon'
names=sorted({json.loads(f.read_text())['icon'] for f in (ROOT/'Apps').glob('*.json')}|{v['icon'] for v in registry.values()})
provenance={'fontawesome_commit':'14c65a3747d0f3b751f15831fc719236aea8729d','reader_commit':'4ff926a4595924f7528147418013ab7f8762db17','license':'SIL OFL 1.1','files':{},'icons':names}
lookup=a.reader/'SD_fonts/FAClassicSolid/FAClassicSolid_codepoints.csv'
cmap={row[0]:row[2] for row in csv.reader(lookup.read_text().splitlines())}
assert cmap['U+F2F2']=='stopwatch', 'Stopwatch must be the actual upstream glyph'
provenance['files'][str(lookup.relative_to(a.reader))]=hashlib.sha256(lookup.read_bytes()).hexdigest()
for value in registry.values():
 assert cmap['U+'+value['icon'].split(':')[1].upper()]==value['glyph'], value
provenance['glyph_names']={v['icon']:v['glyph'] for v in registry.values()}
arrays=[];rows=[]
for style in ('solid','regular'):
 family='FAClassic'+style.title(); path=a.reader/'SD_fonts'/family/(family+'_18.cpfont');data=path.read_bytes();provenance['files'][str(path.relative_to(a.reader))]=hashlib.sha256(data).hexdigest()
 assert data[:8]==b'CPFONT\0\0' and struct.unpack_from('<H',data,8)[0]==4
 _,ni,ng,ay,asc,desc,kl,kr,lc,rc,lig,off=struct.unpack_from('<B3xIIBhhHHBBBI4x',data,32)
 intervals=[struct.unpack_from('<III',data,off+12*i) for i in range(ni)]
 goff=off+ni*12;boff=goff+ng*16+(kl+kr)*3+lc*rc+lig*8
 for name in names:
  if not name.startswith(style+':'):continue
  cp=int(name.split(':')[1],16); matches=[base+cp-lo for lo,hi,base in intervals if lo<=cp<=hi];assert len(matches)==1,name
  w,h,adv,left,top,length,offset=struct.unpack_from('<BBHhhH2xI',data,goff+16*matches[0]);bits=data[boff+offset:boff+offset+length];assert len(bits)==(w*h+3)//4 and w and h
  ident='rpi_'+style+'_'+format(cp,'x');arrays.append('static const uint8_t '+ident+'[] = {'+','.join(map(str,bits))+'};');rows.append('{"%s",%d,%d,%s}'%(name,w,h,ident))
content='/* Generated genuine Font Awesome 7 Free rasters, renamed RiscPortableIcons.\n * Fonticons, Inc.; SIL OFL 1.1. See LICENSE-FontAwesome.txt and SOURCES.json. */\n'+ '\n'.join(arrays)+'\ntypedef struct { const char *name; uint8_t width,height; const uint8_t *bits; } rpi_glyph;\nstatic const rpi_glyph rpi_icons[] = {\n'+',\n'.join(rows)+'\n};\n'
(out/'icons.inc').write_text(content)
arrays=[];rows=[]
for family,size in [('Orbitron-500.ttf',11),('Rajdhani-600.ttf',15)]:
 path=a.fonts/family;data=path.read_bytes();provenance['files'][family]=hashlib.sha256(data).hexdigest();font=ImageFont.truetype(str(path),size)
 start=len(rows)
 for cp in range(32,127):
  ch=chr(cp);mask,offset=font.getmask2(ch,mode='L');w,h=mask.size;packed=[];value=0
  for i,pixel in enumerate(mask):
   value=(value<<2)|(pixel*3//255)
   if i%4==3:packed.append(value);value=0
  if w*h%4:packed.append(value<<(2*(4-w*h%4)))
  if not packed:packed=[0]
  ident='rpt_'+('clock' if size==11 else 'label')+'_'+str(cp)
  arrays.append('static const uint8_t '+ident+'[] = {'+','.join(map(str,packed))+'};')
  rows.append('{%d,%d,%d,%d,%d,%s}'%(w,h,offset[0],offset[1],round(font.getlength(ch)*16),ident))
content='/* Generated RiscPortableText: unmodified Orbitron/Rajdhani raster subsets.\n * Respective OFL licenses and source SHA256 recorded alongside. */\n'+'\n'.join(arrays)+'\ntypedef struct { uint8_t width,height; int8_t left,top; uint16_t advance_q4; const uint8_t *bits; } rpt_glyph;\nstatic const rpt_glyph rpt_text[] = {\n'+',\n'.join(rows)+'\n};\n'
(out/'text.inc').write_text(content)
for name in ['icons.inc','text.inc']:provenance[name+'_sha256']=hashlib.sha256((out/name).read_bytes()).hexdigest()
(out/'SOURCES.json').write_text(json.dumps(provenance,indent=2)+'\n')
(out/'LICENSE-FontAwesome.txt').write_bytes((a.reader/'SD_fonts/FAClassic-LICENSE.txt').read_bytes())
for name in ['Orbitron','Rajdhani']:(out/('LICENSE-'+name+'.txt')).write_bytes((a.fonts/(name+'-OFL.txt')).read_bytes())
print('Generated',len(names),'real FA icons and',len(rows),'text glyphs')
