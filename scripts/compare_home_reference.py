#!/usr/bin/env python3
"""Compare actual mono frames with the supplied SVG rendered using real fonts."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import argparse,json
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--current',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
reference=Image.open(ROOT/'test/native_apps/fixtures/home-interactive-reference.png').convert('L').point(lambda v:255 if v>=128 else 0).convert('1')
before=Image.open(ROOT/'docs/evidence/home-parity/before.png').convert('1');after=Image.open(a.current).convert('1')
assert reference.size==before.size==after.size==(480,800)
regions={'date':(24,16,224,50),'clock':(108,212,372,300),'next':(24,482,456,516),'title':(24,520,456,575),'countdown':(24,580,456,615),'upcoming':(24,662,456,742)}
report={'metric':'black-pixel intersection / union after thresholding the SVG reference at 128','regions':{}}
for name,box in regions.items():
 expected=[v==0 for v in reference.crop(box).getdata()];scores={}
 for label,img in [('before',before),('after',after)]:
  actual=[v==0 for v in img.crop(box).getdata()];union=sum(x or y for x,y in zip(expected,actual));intersection=sum(x and y for x,y in zip(expected,actual));scores[label]=round(intersection/union,4)
 assert scores['after']>=0.8 and scores['after']>scores['before']+0.2,(name,scores)
 report['regions'][name]={'crop':box,**scores}
a.output.mkdir(parents=True,exist_ok=True);(a.output/'reference-comparison.json').write_text(json.dumps(report,indent=2)+'\n')
canvas=Image.new('RGB',(1504,892),'#e8e8e8');draw=ImageDraw.Draw(canvas);font=ImageFont.truetype(str(ROOT/'lib/PortableApps/home_fonts/Rajdhani-700.ttf'),25)
for i,(img,title) in enumerate(zip([reference,before,after],['Interactive reference: same data','Before: existing Home','After: native sizes and weights'])):
 x=16+i*496;draw.text((x,12),title,font=font,fill='black');canvas.paste(img.convert('RGB'),(x,52))
draw.text((16,858),'Deterministic host render. Prototype status icons and instruction footer are omitted in the app.',font=font.font_variant(size=20),fill='black');canvas.save(a.output/'before-after.png')
print('Independent source-reference type/layout comparison PASS:',report['regions'])
