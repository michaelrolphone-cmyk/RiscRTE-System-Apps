#!/usr/bin/env python3
"""Compare actual completed host frames with the supplied Quick Actions SVG render."""
import argparse,hashlib,json,shutil
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--reference',type=Path,required=True);p.add_argument('--before',type=Path,required=True);p.add_argument('--captures',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
 inputs={'reference':a.reference,'before-no-audio':a.before,'after-no-audio':a.captures/'host-frontlight/drawer-open.pbm','after-audio':a.captures/'host-frontlight-audio/drawer-open.pbm','frontlight-persisted':a.captures/'host-frontlight/frontlight-persisted.pbm'}
 images={name:Image.open(path).convert('RGB') for name,path in inputs.items()}
 assert all(im.size==(480,800) for im in images.values())
 for name,im in images.items():im.save(a.output/(name+'.png'))
 ref=np.array(images['reference'].convert('L'))<128;actual=np.array(images['after-audio'].convert('L'))<128
 def iou(box,invert_ref=False,invert_actual=False):
  x,y,w,h=box;r=ref[y:y+h,x:x+w];v=actual[y:y+h,x:x+w]
  if invert_ref:r=~r
  if invert_actual:v=~v
  return round(float((r&v).sum())/max(1,int((r|v).sum())),6)
 metrics={name:iou(box) for name,box in {'title':(30,30,268,34),'frontlight-label':(30,94,230,26),'frontlight-sun':(32,128,40,40),'volume-label':(30,188,230,26),'volume-icon':(32,222,40,40),'battery-label':(30,738,72,36)}.items()}
 # Normalize tile state colors so live values do not become false geometry errors.
 for name,x,y,on_ref,on_actual in [('silent',110,334,True,False),('dnd',322,334,False,False),('airplane',110,438,False,False),('wifi',322,438,True,True),('bluetooth',110,542,True,False),('frontlight',322,542,True,True),('clean',322,636,False,False)]:
  metrics['tile-label-'+name]=iou((x,y,124,50 if name=='clean' else 30),on_ref,on_actual)
 panels=[('reference','Supplied reference'),('before-no-audio','Before: prior host layout'),('after-no-audio','After: X4 without audio'),('after-audio','After: sound-capable fixture')]
 sheet=Image.new('RGB',(480*len(panels),834),'#e8e8e8');draw=ImageDraw.Draw(sheet);font=ImageFont.truetype(str(ROOT/'lib/PortableApps/home_fonts/Rajdhani-700.ttf'),20)
 for i,(key,label) in enumerate(panels):draw.text((i*480+12,4),label,font=font,fill='black');sheet.paste(images[key],(i*480,34))
 sheet.save(a.output/'comparison.png')
 report={'input_sha256':{k:{'path':str(v.resolve()),'sha256':sha(v)} for k,v in inputs.items()},'one_bit_iou':metrics,'reference_font_manifest_sha256':sha(ROOT/'lib/PortableApps/quick_fonts/REFERENCE.json'),'dynamic_values_compared':False,'differences':['No sound hardware omits VOLUME/SILENT and compacts the grid.','LOW POWER is absent because no manual mode operation/behavior is assigned.','USB is a matching grid tile in both no-audio and audio profiles.','Time, brightness, volume, battery and selected radio/mute states are confirmed fixture values rather than the static sample.','Dynamic text uses the exact font subset with one-bit glyph hinting; static labels/icons use the reference-shaped masks.'],'hardware_verified':False,'source':'Actual completed host frames from scripts/test_shared_quick_reference.py; no reconstructed product screenshots'}
 (a.output/'comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(metrics,indent=2))
if __name__=='__main__':main()
