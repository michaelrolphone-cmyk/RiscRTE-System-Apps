#!/usr/bin/env python3
"""Measure lossless image encodings only; no deployment decoder or format claim."""
import argparse,hashlib,json,zlib
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
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--elf',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 raw=a.elf.read_bytes();packed=lz4_block(raw)
 if decode(packed,len(raw))!=raw:raise ValueError('LZ4 block roundtrip differs')
 co=zlib.compressobj(9,zlib.DEFLATED,-15);deflate=co.compress(raw)+co.flush()
 if zlib.decompress(deflate,-15)!=raw:raise ValueError('Deflate roundtrip differs')
 result={'schema':1,'measurement_only':True,'input_bytes':len(raw),'input_sha256':hashlib.sha256(raw).hexdigest(),'lz4_block_bytes':len(packed),'raw_deflate_bytes':len(deflate),'deployment_decoder_qualified':False}
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
