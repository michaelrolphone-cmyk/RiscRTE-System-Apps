#!/usr/bin/env python3
"""Qualify one modern compiler for the complete selected Home profile.
No source profile, features, SDK, runtime ABI, or LTO objects are substituted.
Each alternative is explicit compiler/linker code generation, structurally
validated by the owning recipe. An unsuccessful attempt is retained as evidence.
"""
import argparse,hashlib,json,os,shlex,subprocess,sys
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for name in ('x4','runtime','reader','utilities','compiler','output'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();root=Path(__file__).resolve().parents[1];out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);cc=a.compiler.resolve()
attempts=[];chosen=None
variants=[('no-ivopts',['-fno-ivopts']),('no-switch-tables',['-fno-ivopts','-fno-jump-tables','-fno-tree-switch-conversion']),('separate-literals',['-mno-text-section-literals']),('link-relax',['-Wl,--relax']),('optimization-one',['-O1']),('optimization-zero',['-O0'])]
for label,flags in variants:
 directory=out/label;bin=directory/'compiler';bin.mkdir(parents=True,exist_ok=True)
 wrapper=bin/cc.name;wrapper.write_text('#!/bin/sh\nexec '+shlex.quote(str(cc))+' "$@" '+shlex.join(flags)+'\n');wrapper.chmod(0o755)
 for name in ('nm','readelf','objdump'):
  dest=bin/(cc.name.removesuffix('gcc')+name);dest.symlink_to(cc.parent/dest.name)
 command=[sys.executable,str(a.x4/'minimal/recovery-107/build_clock.py'),'--system',str(root),'--x4',str(a.x4),'--runtime',str(a.runtime),'--sdk',str(a.reader/'sdk/driver'),'--utilities',str(a.utilities),'--reader-scripts',str(a.reader/'scripts'),'--compiler',str(wrapper),'--output',str(directory/'target')]
 result=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 (directory/'build.log').write_text(result.stdout)
 attempts.append({'label':label,'flags':flags,'command':command,'returncode':result.returncode})
 print(label,'PASS' if result.returncode==0 else 'FAILED',flush=True)
 if result.returncode:print('\n'.join(result.stdout.splitlines()[-45:]),flush=True)
 if result.returncode==0:
  repeat=command[:-1]+[str(directory/'repeat')];second=subprocess.run(repeat,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
  (directory/'repeat.log').write_text(second.stdout);second.check_returncode()
  elf=directory/'target/home/default.elf';assert elf.read_bytes()==(directory/'repeat/home/default.elf').read_bytes()
  chosen={'label':label,'flags':flags,'elf_sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),'size_bytes':elf.stat().st_size,'repeat_identical':True};break
receipt={'compiler':subprocess.check_output([str(cc),'--version'],text=True),'compiler_sha256':hashlib.sha256(cc.read_bytes()).hexdigest(),'attempts':attempts,'selected':chosen,'hardware_tested':False}
(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
assert chosen,'All modern code-generation alternatives failed; inspect archived build logs'
print(json.dumps(chosen),flush=True)
