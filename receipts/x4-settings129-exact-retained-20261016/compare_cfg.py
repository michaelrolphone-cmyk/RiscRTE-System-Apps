import sys, subprocess,re,struct,json,collections
from sections import ELF
OBJDUMP=TARGET=CANDIDATE=None
class Binary:
 def __init__(self,path):
  self.path=path;self.elf=ELF(path);self.ins={}; self.rel={}
  for sn in ['.rela.dyn','.rela.plt']:
   for o,i,a in struct.iter_unpack('<III',self.elf.get(sn)): self.rel[o]=(i,a)
 def data(self,addr,size):
  for n,s in self.elf.sections.items():
   if s[1]!=8 and s[3]<=addr and addr+size<=s[3]+s[5]:return self.elf.get(n)[addr-s[3]:addr-s[3]+size]
  raise ValueError(hex(addr))
 def inst(self,a):
  if a in self.ins:return self.ins[a]
  r=subprocess.check_output([OBJDUMP,'-d','--start-address='+hex(a),'--stop-address='+hex(a+3),self.path],text=True)
  line=next(s for s in r.splitlines() if re.match(r'\s*'+format(a,'x')+':',s))
  m=re.match(r'\s*([0-9a-f]+):\s+([0-9a-f]+)\s+(\S+)\s*(.*)',line)
  if not m:raise ValueError(line)
  a=int(m[1],16);n=len(m[2])//2;op=m[3];args=re.sub(r' <[^>]+>','',m[4]).strip();self.ins[a]=(n,op,args);return self.ins[a]
 def norm(self,a):
  n,op,args=self.inst(a);op=op.removesuffix('.n');targets=[];follows=[]
  if op=='j': targets=[int(args,16)]
  elif op.startswith('b') or op.startswith('loop'):
   op=op # all integer branch instructions
   parts=args.split(', ');targets=[int(parts.pop(),16)];args=', '.join(parts);follows=[a+n]
  elif op.startswith('ret'):pass
  else:follows=[a+n]
  if op=='l32r':
   reg,lit=args.split(', '); lit=int(lit,16);v=struct.unpack('<I',self.data(lit,4))[0];args=f'{reg}, literal_value={v:x}, reloc={self.rel.get(lit)}'
  if op=='or':
   p=args.split(', ')
   if p[1]==p[2]:op='mov';args=', '.join(p[:2])
  return op,args,targets,follows
 def skipj(self,a):
  seen=set()
  while self.inst(a)[1]=='j':
   if a in seen:raise ValueError('jump cycle')
   seen.add(a);a=int(self.inst(a)[2],16)
  return a

def compare(an,bn):
 A=Binary(TARGET);B=Binary(CANDIDATE);queue=[(an,bn)];seen=set();mismatches=[]; amap={};bmap={};paired=[]
 while queue:
  a,b=queue.pop();a=A.skipj(a);b=B.skipj(b)
  if (a,b) in seen:continue
  seen.add((a,b))
  if (a in amap and amap[a]!=b) or (b in bmap and bmap[b]!=a):mismatches.append({'target':hex(a),'candidate':hex(b),'reason':'inconsistent CFG mapping'});continue
  amap[a]=b;bmap[b]=a
  ao,aa,at,af=A.norm(a);bo,ba,bt,bf=B.norm(b)
  paired.append({'target':hex(a),'candidate':hex(b),'target_ins':A.inst(a),'candidate_ins':B.inst(b)})
  if (ao,aa,len(at),len(af))!=(bo,ba,len(bt),len(bf)):
   mismatches.append({'target':hex(a),'candidate':hex(b),'target_norm':(ao,aa),'candidate_norm':(bo,ba)})
   # Continue only if instructions have compatible topology, keeping diagnostics finite.
   if len(at)!=len(bt) or len(af)!=len(bf):continue
  queue.extend(zip(at,bt));queue.extend(zip(af,bf))
 return {'target_start':hex(an),'candidate_start':hex(bn),'compared_nodes':len(seen),'mismatches':mismatches,'paired':paired}
if __name__=='__main__':
 import argparse
 p=argparse.ArgumentParser(description='Compare reachable Xtensa instruction CFGs from exact entry addresses.')
 p.add_argument('--objdump',required=True);p.add_argument('--target',required=True);p.add_argument('--candidate',required=True)
 p.add_argument('--target-entry',required=True,type=lambda s:int(s,0));p.add_argument('--candidate-entry',required=True,type=lambda s:int(s,0));p.add_argument('--output',required=True)
 a=p.parse_args();OBJDUMP=a.objdump;TARGET=a.target;CANDIDATE=a.candidate
 r=compare(a.target_entry,a.candidate_entry);json.dump(r,open(a.output,'w'),indent=2);print(json.dumps({k:v for k,v in r.items() if k!='paired'},indent=2))
