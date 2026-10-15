#!/usr/bin/env python3
"""Pack original audited font outlines; render them directly at native pixels.

Requires fontTools and Brotli (or the platform libbrotlidec), never downloads.
All inputs and OFL licenses are committed beside the generated source.
"""
import ctypes,ctypes.util,hashlib,json,sys,types
from pathlib import Path
try:
 import brotli
 decoder={"implementation":"python-brotli","version":brotli.__version__}
except ImportError:
 lib=ctypes.CDLL(ctypes.util.find_library('brotlidec'))
 version=lib.BrotliDecoderVersion()
 decoder={"implementation":"libbrotlidec","version":f'{version>>24}.{(version>>12)&4095}.{version&4095}'}
 lib.BrotliDecoderDecompress.argtypes=[ctypes.c_size_t,ctypes.c_void_p,ctypes.POINTER(ctypes.c_size_t),ctypes.c_void_p]
 def decompress(data):
  out=ctypes.create_string_buffer(16*1024*1024);size=ctypes.c_size_t(len(out))
  assert lib.BrotliDecoderDecompress(len(data),data,ctypes.byref(size),out)==1
  return out.raw[:size.value]
 sys.modules['brotli']=types.SimpleNamespace(decompress=decompress)
from fontTools.ttLib import TTFont
from fontTools.pens.basePen import BasePen
from fontTools.pens.boundsPen import BoundsPen
import fontTools
ROOT=Path(__file__).resolve().parents[1];out=ROOT/'lib/PortableApps/fonts';source=out/'native_sources'
pins=json.loads((out/'SOURCES.json').read_text());raw=bytearray();rows=[];icons=[];text=[];sources={}
class OutlinePen(BasePen):
 def __init__(self,gs):super().__init__(gs);self.data=bytearray();self.x=self.y=0
 def point(self,p):
  x,y=map(round,p)
  for v in (x-self.x,y-self.y):
   v=(v<<1)^(v>>31)
   while v>=128:self.data.append((v&127)|128);v>>=7
   self.data.append(v)
  self.x,self.y=x,y
 def _moveTo(self,p):self.data.append(0);self.point(p)
 def _lineTo(self,p):self.data.append(1);self.point(p)
 def _qCurveToOne(self,p,q):self.data.append(2);self.point(p);self.point(q)
 def _closePath(self):self.data.append(3)
 def _curveToOne(self,p,q,r):self.data.append(4);self.point(p);self.point(q);self.point(r)
def read_font(name):
 path=source/name;data=path.read_bytes();digest=hashlib.sha256(data).hexdigest()
 if name.endswith('.ttf'):assert digest==pins['files'][name]
 else:
  expected='0097ffd06bdd71a60679ad9474bfa97e1e90201f' if 'solid' in name else 'e7c86422b686a8a304869355070e127eac2ad2b2'
  assert hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()==expected
 sources[name]=digest;return TTFont(path)
def glyph(font,cp):
 gs=font.getGlyphSet();name=font.getBestCmap().get(cp,'.notdef');p=OutlinePen(gs);gs[name].draw(p)
 bounds=BoundsPen(gs);gs[name].draw(bounds);box=bounds.bounds or (0,0,0,0);box=tuple(round(v) for v in box)
 index=len(rows);rows.append((len(raw),len(p.data),*box,round(gs[name].width),font['head'].unitsPerEm,font['hhea'].ascent));raw.extend(p.data);return index
for style,weight in [('solid','900'),('regular','400')]:
 f=read_font(f'fa-{style}-{weight}.woff2')
 for name in pins['icons']:
  if name.startswith(style+':'):icons.append((name,glyph(f,int(name.split(':')[1],16))))
for name in ['Orbitron-500.ttf','Rajdhani-600.ttf']:
 f=read_font(name);ids={}
 for cp in range(32,127):
  mapped=cp-32 if 97<=cp<=122 else cp
  if mapped not in ids:ids[mapped]=glyph(f,mapped)
  text.append(ids[mapped])
# LZSS: groups of8 tokens; literal flag1, otherwise12-bit backwards distance-1
# and4-bit length-3. Decode has fixed source/output bounds and no allocation.
packed=bytearray();positions={};at=0
while at<len(raw):
 flag_at=len(packed);packed.append(0);flags=0
 for bit in range(8):
  if at>=len(raw):break
  key=bytes(raw[at:at+3]);best=0;distance=0
  for old in reversed(positions.get(key,[])[-128:]):
   if at-old>4096:break
   n=0
   while n<18 and at+n<len(raw) and raw[old+n]==raw[at+n]:n+=1
   if n>best:best=n;distance=at-old
   if n==18:break
  length=best if best>=3 else 1
  if best>=3:
   token=((distance-1)<<4)|(best-3);packed.extend((token&255,token>>8))
  else:flags|=1<<bit;packed.append(raw[at])
  for n in range(length):positions.setdefault(bytes(raw[at+n:at+n+3]),[]).append(at+n)
  at+=length
 packed[flag_at]=flags
content='''/* Original audited Orbitron, Rajdhani and Font Awesome outlines.
 * Lossless delta-varint commands + bounded LZSS, no pre-rasterized view.
 * See native_vectors_sources.json and existing SIL OFL license files. */
typedef struct { uint16_t offset,length; int16_t left,bottom,right,top; uint16_t advance,units; int16_t ascent; } np_vector_glyph;
'''
content+=f'#define NP_VECTOR_BYTES {len(raw)}u\nstatic const uint8_t np_vector_packed[]={{'+','.join(map(str,packed))+'};\n'
content+='static const np_vector_glyph np_vector_glyphs[]={\n'+',\n'.join('{'+','.join(map(str,r))+'}' for r in rows)+'\n};\n'
content+='static const struct { const char *name; uint8_t glyph; } np_vector_icons[]={\n'+',\n'.join('{"%s",%d}'%v for v in icons)+'\n};\n'
content+='static const uint8_t np_vector_text[]={'+','.join(map(str,text))+'};\n'
assert len(rows)<256 and len(raw)<65536
(out/'native_vectors.inc').write_text(content)
(out/'native_vectors_sources.json').write_text(json.dumps({'sources':sources,'fontawesome_commit':pins['fontawesome_commit'],'text_source_repository':'michaelrolphone-cmyk/RiscRTE-Watch','text_source_commit':'6e82fdcaec0b73ed700d2104cfb4d94f011c19d4','text_source_path':'apps/clock/nova/fonts','generator':{'fonttools':fontTools.__version__,'python':sys.version.split()[0],'brotli':decoder},'glyphs':len(rows),'raw_bytes':len(raw),'packed_bytes':len(packed),'encoding':'delta-varint vector commands; LZSS4096/18','raw_sha256':hashlib.sha256(raw).hexdigest(),'packed_sha256':hashlib.sha256(packed).hexdigest(),'source_sha256':hashlib.sha256(content.encode()).hexdigest()},indent=2)+'\n')
print('native vectors:',len(rows),'glyphs;',len(raw),'raw bytes;',len(packed),'packed bytes')
