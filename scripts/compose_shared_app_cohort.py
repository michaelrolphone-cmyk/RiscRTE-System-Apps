#!/usr/bin/env python3
"""Compose independently qualified product deltas without reverting other apps.
Each input is a hosted qualification's products directory containing lane/before
and lane/after inventories. Conflicting edits are errors, never last-writer-wins.
Archive identities must be checked by the owning workflow before extraction.
"""
import argparse, copy, hashlib, io, json, zipfile
from pathlib import Path, PurePosixPath

def safe(name):
 p=PurePosixPath(name)
 if not name or p.is_absolute() or '..' in p.parts or '\\' in name: raise ValueError('Unsafe path '+name)
 return name

def tree(path):
 if not path.is_dir(): raise ValueError('Missing inventory '+str(path))
 return {safe(p.relative_to(path).as_posix()):p.read_bytes() for p in path.rglob('*') if p.is_file()}

def unzip(path):
 with zipfile.ZipFile(path) as z:
  names=z.namelist()
  if len(names)!=len(set(names)): raise ValueError('Duplicate archive paths')
  for n in names:safe(n)
  return {n:z.read(n) for n in names if not n.endswith('/')}

def encode(v): return (json.dumps(v,sort_keys=True,indent=2)+'\n').encode()
def digest(v): return hashlib.sha256(v).hexdigest()

def merge_rows(original, current, proposed, key):
 def index(rows):
  result={}
  for row in rows:
   ident=row.get(key)
   if not isinstance(ident,str) or ident in result: raise ValueError('Ambiguous authority '+key)
   result[ident]=row
  return result
 old=index(original); now=index(current); new=index(proposed)
 for ident in set(old)|set(new):
  if old.get(ident)==new.get(ident):continue
  if now.get(ident)!=old.get(ident) and now.get(ident)!=new.get(ident):raise ValueError('Conflicting authority '+ident)
  if ident in new:now[ident]=copy.deepcopy(new[ident])
  else:now.pop(ident,None)
 # Preserve prior order; append genuinely new rows in their published order.
 order=[r[key] for r in current if r[key] in now]
 order += [r[key] for r in proposed if r[key] not in order and r[key] in now]
 return [now[k] for k in order]

def merge_boot(original,current,proposed):
 old=json.loads(original);now=json.loads(current);new=json.loads(proposed)
 for key in set(old)|set(new):
  if old.get(key)==new.get(key):continue
  if key in ('drivers','app_capabilities'):
   now[key]=merge_rows(old.get(key,[]),now.get(key,[]),new.get(key,[]),'manifest')
  elif now.get(key)==old.get(key) or now.get(key)==new.get(key):
   if key in new:now[key]=copy.deepcopy(new[key])
   else:now.pop(key,None)
  else:raise ValueError('Conflicting boot field '+key)
 return encode(now)

def apply(original,current,proposed):
 out=dict(current)
 for name in sorted(set(original)|set(proposed)):
  old=original.get(name);new=proposed.get(name)
  if old==new:continue
  if name=='boot.json' and old is not None and new is not None:
   out[name]=merge_boot(old,out[name],new);continue
  if out.get(name)!=old and out.get(name)!=new:raise ValueError('Conflicting file '+name)
  if new is None:out.pop(name,None)
  else:out[name]=new
 return out

def write_tree(path,entries):
 for name,data in entries.items():
  p=path/safe(name);p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)

def compose(products,baseline_sd,output):
 if output.exists():raise ValueError('Output exists')
 receipts={}
 for lane in ('watch','wave','x4'):
  before=tree(products[0]/lane/'before');after=dict(before)
  original_sd=unzip(baseline_sd/(lane+'.zip')) if (baseline_sd/(lane+'.zip')).exists() else {}
  after_sd=dict(original_sd);sources=[]
  for product in products:
   old=tree(product/lane/'before');new=tree(product/lane/'after')
   if old!=before:raise ValueError('Product baselines differ: '+str(product)+' '+lane)
   after=apply(old,after,new)
   new_sd=unzip(product/lane/'sd-apps.zip') if (product/lane/'sd-apps.zip').exists() else dict(original_sd)
   if (product/lane/'before-sd').is_dir() and tree(product/lane/'before-sd')!=original_sd:raise ValueError('SD baseline differs')
   after_sd=apply(original_sd,after_sd,new_sd)
   sources.append({'inventory':str(product),'changed_internal':[n for n in sorted(set(old)|set(new)) if old.get(n)!=new.get(n)],
                   'changed_sd':[n for n in sorted(set(original_sd)|set(new_sd)) if original_sd.get(n)!=new_sd.get(n)]})
  for name,entries in (('before',before),('after',after),('before-sd',original_sd),('after-sd',after_sd)):
   write_tree(output/lane/name,entries)
  receipts[lane]={'sources':sources,'files':{n:digest(b) for n,b in sorted(after.items())},
                  'sd_files':{n:digest(b) for n,b in sorted(after_sd.items())}}
 output.mkdir(parents=True,exist_ok=True)
 (output/'composition.json').write_bytes(encode({'schema':1,'products':receipts,'packed':False}))
 return receipts

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--product',action='append',type=Path,required=True)
 p.add_argument('--baseline-sd',type=Path,required=True)
 p.add_argument('--output',type=Path,required=True)
 a=p.parse_args();compose(a.product,a.baseline_sd,a.output)
