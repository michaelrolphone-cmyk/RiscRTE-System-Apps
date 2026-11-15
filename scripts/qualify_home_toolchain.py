#!/usr/bin/env python3
"""Qualify the unchanged selected Home profile, preserving partial evidence."""
import argparse,hashlib,json,os,shlex,subprocess,sys
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for name in ('x4','runtime','reader','utilities','compiler','output'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();root=Path(__file__).resolve().parents[1];out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);cc=a.compiler.resolve()
attempts=[];chosen=None
receipt={'compiler':None,'compiler_sha256':None,'attempts':attempts,'selected':None,'hardware_tested':False,'state':'prepared','compiler_observation':'pending'}
def save():
 temporary=out/'receipt.json.tmp'
 temporary.write_text(json.dumps(receipt,indent=2)+'\n')
 temporary.replace(out/'receipt.json')
save()
variants=[('no-ivopts',['-fno-ivopts']),('no-switch-tables',['-fno-ivopts','-fno-jump-tables','-fno-tree-switch-conversion']),('separate-literals',['-mno-text-section-literals']),('link-relax',['-Wl,--relax']),('optimization-one',['-O1']),('optimization-zero',['-O0'])]
attempt=None
try:
 for label,flags in variants:
  directory=out/label;bin=directory/'compiler'
  command=[sys.executable,str(a.x4/'minimal/recovery-107/build_clock.py'),'--system',str(root),'--x4',str(a.x4),'--runtime',str(a.runtime),'--sdk',str(a.reader/'sdk/driver'),'--utilities',str(a.utilities),'--reader-scripts',str(a.reader/'scripts'),'--compiler',str(bin/cc.name),'--output',str(directory/'target')]
  attempt={'label':label,'flags':flags,'command':command,'returncode':None,'state':'preparing'}
  attempts.append(attempt);receipt['state']='running';save()
  bin.mkdir(parents=True,exist_ok=True)
  wrapper=bin/cc.name;wrapper.write_text('#!/bin/sh\nexec '+shlex.quote(str(cc))+' "$@" '+shlex.join(flags)+'\n');wrapper.chmod(0o755)
  for name in ('nm','readelf','objdump'):
   dest=bin/(cc.name.removesuffix('gcc')+name);dest.symlink_to(cc.parent/dest.name)
  attempt['state']='initial-invocation';save()
  result=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
  attempt.update(returncode=result.returncode,state='initial-returned');save()
  (directory/'build.log').write_text(result.stdout)
  attempt['build_log']=str(directory/'build.log');save()
  print(label,'PASS' if result.returncode==0 else 'FAILED',flush=True)
  if result.returncode:
   attempt['state']='initial-failed';save()
   print('\n'.join(result.stdout.splitlines()[-45:]),flush=True)
  if result.returncode==0:
   repeat=command[:-1]+[str(directory/'repeat')]
   attempt.update(state='repeat-invocation',repeat_command=repeat,repeat_returncode=None);save()
   second=subprocess.run(repeat,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
   attempt.update(state='repeat-returned',repeat_returncode=second.returncode);save()
   (directory/'repeat.log').write_text(second.stdout)
   attempt['repeat_log']=str(directory/'repeat.log');save()
   second.check_returncode()
   attempt['state']='comparing';save()
   elf=directory/'target/home/default.elf';identical=elf.read_bytes()==(directory/'repeat/home/default.elf').read_bytes()
   attempt['repeat_identical']=identical;save()
   assert identical
   chosen={'label':label,'flags':flags,'elf_sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),'size_bytes':elf.stat().st_size,'repeat_identical':True}
   receipt['selected']=chosen;attempt['state']='passed';save();break
 receipt['state']='compiler-observation';save()
 receipt['compiler']=subprocess.check_output([str(cc),'--version'],text=True)
 receipt['compiler_sha256']=hashlib.sha256(cc.read_bytes()).hexdigest();receipt['compiler_observation']='complete';save()
 assert chosen,'All modern code-generation alternatives failed; inspect archived build logs'
 receipt['state']='passed';save()
 print(json.dumps(chosen),flush=True)
except BaseException as error:
 receipt['failure']={'type':type(error).__name__,'message':str(error),'stage':receipt['state'],'attempt_state':attempt['state'] if attempt else None}
 receipt['state']='failed'
 try:save()
 except OSError as evidence_error:
  print('Unable to preserve failure receipt: '+str(evidence_error),file=sys.stderr)
 raise
