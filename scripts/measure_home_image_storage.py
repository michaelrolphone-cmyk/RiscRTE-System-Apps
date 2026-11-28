#!/usr/bin/env python3
"""Measure lossless image encodings only; no deployment decoder or format claim."""
import argparse,hashlib,json,zlib,sys
from pathlib import Path
def lz4_block(data):
 out=bytearray();table={};anchor=pos=0;n=len(data)
 def length(value):
  while value>=255:out.append(255);value-=255
  out.append(value)
 while pos+12<=n:
  key=data[pos:pos+4];previous=table.get(key);table[key]=pos
  if previous is None or pos-previous>65535:
   pos+=1;continue
  end=pos+4
  # Preserve five terminal literals and the standard last-match constraint.
  while end<n-5 and data[previous+end-pos]==data[end]:end+=1
  literals=pos-anchor;match=end-pos-4
  out.append((min(literals,15)<<4)|min(match,15))
  if literals>=15:length(literals-15)
  out.extend(data[anchor:pos]);out.extend((pos-previous).to_bytes(2,'little'))
  if match>=15:length(match-15)
  pos=end;anchor=pos
 tail=n-anchor;out.append(min(tail,15)<<4)
 if tail>=15:length(tail-15)
 out.extend(data[anchor:]);return bytes(out)
def decode(block,size):
 out=bytearray();pos=0
 def length(base):
  nonlocal pos
  if base==15:
   while True:
    if pos>=len(block):raise ValueError('Truncated length')
    value=block[pos];pos+=1;base+=value
    if value!=255:break
  return base
 while pos<len(block):
  token=block[pos];pos+=1;count=length(token>>4)
  if count>len(block)-pos or count>size-len(out):raise ValueError('Literal bounds')
  out.extend(block[pos:pos+count]);pos+=count
  if pos==len(block):break
  if pos+2>len(block):raise ValueError('Truncated offset')
  distance=int.from_bytes(block[pos:pos+2],'little');pos+=2
  if not distance or distance>len(out):raise ValueError('Invalid distance')
  count=length(token&15)+4
  if count>size-len(out):raise ValueError('Match bounds')
  for _ in range(count):out.append(out[-distance])
 if len(out)!=size:raise ValueError('Output length differs')
 return bytes(out)
def cohort_measure(root,tools,out):
 from compose_shared_app_cohort import tree
 sys.path.insert(0,str(tools.resolve()))
 from current_bootfs import build
 from read_only_spiffs import read_image
 entries=tree(root/'watch/after');size=json.loads((root/'watch/payload.json').read_text())['store_bytes']
 rows={};encoded={'lz4':{},'deflate':{}}
 boot=json.loads(entries['boot.json']);ordinary=set()
 for row in boot.get('app_capabilities',[]):
  manifest=json.loads(entries[row['manifest']])
  if manifest.get('type')!='application':raise ValueError('App policy names non-application')
  image=row.get('image_source')
  if image and not image.get('internal_fallback'):continue
  name=str(Path(row['manifest']).parent/manifest['file_name'])
  if name not in entries:raise ValueError('Missing ordinary application image')
  ordinary.add(name)
 for name in sorted(ordinary):
  raw=entries[name]
  blocks={'lz4':lz4_block(raw)}
  c=zlib.compressobj(9,zlib.DEFLATED,-15);blocks['deflate']=c.compress(raw)+c.flush()
  if decode(blocks['lz4'],len(raw))!=raw or zlib.decompress(blocks['deflate'],-15)!=raw:raise ValueError('Roundtrip differs '+name)
  rows[name]={'original':len(raw),'sha256':hashlib.sha256(raw).hexdigest()}
  for codec,data in blocks.items():
   # Hypothetical fixed header cost only: NOT a deployment format.
   encoded[codec][name]=b'\0'*32+data;rows[name][codec]=len(data)+32
 results={}
 for codec in encoded:
  candidate=dict(entries);selected=[]
  # General largest-positive-saving selection; no features, files or data removed.
  options=sorted(encoded[codec],key=lambda name:(-(len(entries[name])-len(encoded[codec][name])),name))
  for name in options:
   if len(encoded[codec][name])>=len(entries[name]):continue
   candidate[name]=encoded[codec][name];selected.append(name)
   try:
    image,record=build(candidate,size)
    if read_image(image,size)!=candidate:raise ValueError('Packing roundtrip differs')
    results[codec]={'selected':selected.copy(),'packing':record,'recovered_originals_verified':True,'deployment_qualified':False}
    break
   except Exception as error:
    if type(error).__name__!='SpiffsFullError' and 'free-block reserve' not in str(error):raise
  if codec not in results:results[codec]={'fits':False,'selected':selected,'deployment_qualified':False}
 report={'measurement_only':True,'files':rows,'strategies':results}
 (out.parent/'cohort-storage.json').write_text(json.dumps(report,sort_keys=True,indent=2)+'\n')
 print('HYPOTHETICAL STORAGE MEASUREMENT '+json.dumps(report),flush=True)

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--elf',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--assembly',type=Path);p.add_argument('--bootfs-tools',type=Path);a=p.parse_args()
 raw=a.elf.read_bytes();packed=lz4_block(raw)
 if decode(packed,len(raw))!=raw:raise ValueError('LZ4 block roundtrip differs')
 co=zlib.compressobj(9,zlib.DEFLATED,-15);deflate=co.compress(raw)+co.flush()
 if zlib.decompress(deflate,-15)!=raw:raise ValueError('Deflate roundtrip differs')
 result={'schema':1,'measurement_only':True,'input_bytes':len(raw),'input_sha256':hashlib.sha256(raw).hexdigest(),'lz4_block_bytes':len(packed),'raw_deflate_bytes':len(deflate),'deployment_decoder_qualified':False}
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
 if a.assembly or a.bootfs_tools:
  if not(a.assembly and a.bootfs_tools):raise ValueError('Both assembly and tools required')
  cohort_measure(a.assembly,a.bootfs_tools,a.output)
if __name__=='__main__':main()
