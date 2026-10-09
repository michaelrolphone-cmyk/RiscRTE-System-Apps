"""Independent asset geometry/glyph oracle in native 800x480 MONO1 layout."""
import re,xml.etree.ElementTree as ET,hashlib,json

def expected_panel(asset):
 source=(asset/'RiscRteLogo.h').read_text();result=bytearray(48000)
 metadata=json.loads((asset/'SOURCES.json').read_text())
 assert hashlib.sha256((asset/'logo.svg').read_bytes()).hexdigest()==metadata['sources']['docs/logo.svg']
 def pixel(x,y):
  px,py=y,479-x
  result[py*100+px//8]|=0x80>>(px%8)
 for node in ET.parse(asset/'logo.svg').getroot().iter('rect'):
  x,y,w,h=[int(node.attrib[k])*2 for k in ('x','y','width','height')]
  for py in range(h):
   for px in range(w):
    cx=3 if px<4 else w-4;cy=3 if py<4 else h-4
    if 4<=px<w-4 or 4<=py<h-4 or (px-cx)**2+(py-cy)**2<=16:pixel(120+x+px,240+y+py)
 widths=list(map(int,re.search(r'Widths\[7\]=\{(.*?)\}',source).group(1).split(',')))
 rows=re.search(r'Rows\[7\]\[32\]=\{(.*?)\n\};',source,re.S).group(1);x=(480-sum(widths))//2
 values=widths+[int(v[:-1],16) for v in re.findall(r'0x[0-9a-f]+u',rows)]
 assert hashlib.sha256(json.dumps(values,separators=(',',':')).encode()).hexdigest()==metadata['wordmark_values_sha256']
 for width,row in zip(widths,re.findall(r'\{(.*?)\}',rows)):
  for y,value in enumerate(re.findall(r'0x[0-9a-f]+u',row)):
   for px in range(width):
    if int(value[:-1],16)&(1<<px):pixel(x+px,484+y)
  x+=width
 return bytes(result)
