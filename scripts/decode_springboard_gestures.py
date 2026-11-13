#!/usr/bin/env python3
"""Decode explicit Springboard gesture-diagnostic schema1 from an RTE_PERF dump.
These are bounded terminal summaries, not generic metrics or physical timestamps.
Incomplete/overwritten groups are reported, never filled from adjacent gestures.
"""
import argparse,json,re
PATTERN=re.compile(r'^RTE_PERF seq=(\d+) us=(\d+) interaction=(\d+) invocation=(\d+) phase=(\d+) value=(\d+)\s*$')
def decode(lines):
 groups=[];current=None;incomplete=0
 for line in lines:
  m=PATTERN.match(line)
  if not m:continue
  seq,us,interaction,invocation,phase,value=map(int,m.groups())
  if phase!=8 or not 0xe0<=value>>24<=0xe4:continue
  if value>0xffffffff:raise ValueError('diagnostic word outside uint32')
  tag=value>>24
  if tag==0xe0:
   if current is not None:incomplete+=1
   if ((value>>21)&7)!=1:raise ValueError('unsupported diagnostic schema')
   current={'schema':1,'classification':'Springboard-terminal-gesture-diagnostic','interaction':interaction,'invocation':invocation,'emitted_us':us,'width':((value>>10)&1023)+1,'height':(value&1023)+1,'rotation':180 if value&(1<<20) else 0,'sequence':seq,'words':[],'expected':None}
   continue
  if current is None:incomplete+=1;continue
  if (interaction,invocation,seq)!=(current['interaction'],current['invocation'],current['sequence']+1):incomplete+=1;current=None;continue
  current['sequence']=seq
  if tag==0xe1:
   if current['words'] or current['expected'] is not None or value&0x00fff000:raise ValueError('invalid contact header')
   mask=(value>>8)&15
   if mask&9!=9 or mask&4 and not mask&2:raise ValueError('invalid semantic point mask')
   current['contact_id']=value&255;current['mask']=mask;current['expected']=3*mask.bit_count();continue
  if current['expected'] is None:raise ValueError('missing contact header')
  index=len(current['words']);slots=[i for i in range(4) if current['mask']&(1<<i)]
  if index>=current['expected']:raise ValueError('too many diagnostic words')
  slot=slots[index//3];kind=0 if slot==0 else 2 if slot==3 else 1
  if tag!=0xe2+index%3 or (value>>22)&3!=slot or (value>>20)&3!=kind:raise ValueError('diagnostic word order mismatch')
  current['words'].append(value)
  if len(current['words'])==current['expected']:
   points=[]
   for j,slot in enumerate(slots):
    raw,mapped,delta=current['words'][j*3:j*3+3]
    px,py=(raw>>10)&1023,raw&1023;x,y=(mapped>>10)&1023,mapped&1023
    if px>=current['width'] or x>=current['width'] or py>=current['height'] or y>=current['height']:raise ValueError('coordinate outside declared geometry')
    dx,dy=(delta>>10)&1023,delta&1023;dx=dx-1024 if dx&512 else dx;dy=dy-1024 if dy&512 else dy
    if abs(dx)>256 or abs(dy)>256:raise ValueError('delta outside actual clamp')
    points.append({'semantic':['down','first-motion','first-reversal','release'][slot],'decoded_provider':[px,py],'delivered_mapped':[x,y],'dispatched_delta':[dx,dy]})
   current['points']=points;current.pop('words');current.pop('expected');current.pop('mask');groups.append(current);current=None
 if current is not None:incomplete+=1
 return {'schema':1,'gestures':groups,'incomplete_groups_or_orphan_records':incomplete,'physical_timestamp':False,'raw_CST_bytes':False,'metrics':False}
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('log');a=p.parse_args()
 with open(a.log) as f:print(json.dumps(decode(f),indent=2))
if __name__=='__main__':main()
