#!/usr/bin/env python3
"""Strict additive-header compatibility; no Runtime/host execution claim."""
import argparse, hashlib, json, os, shlex, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--runtime',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--sanitize',action='store_true')
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
runtime=a.runtime.resolve();shell=runtime/'sdk/app/RiscResidentShellV1.h'
raw=shell.read_bytes()
assert hashlib.sha1(b'blob '+str(len(raw)).encode()+b'\0'+raw).hexdigest()=='26356c2b0a836c9d73efda058c80a71c3233fc38','Not the exact Runtime0052 original shell header'
source=ROOT/'test/resident_alarm/header_test.c'
commands=[];inputs={str(Path(__file__).resolve()):hashlib.sha256(Path(__file__).read_bytes()).hexdigest()};binaries={};layouts={}
def run(cmd,label):
 commands.append(cmd)
 (out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
 r=subprocess.run(cmd,text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
 (out/(label+'.log')).write_text(r.stdout+r.stderr);r.check_returncode();return r.stdout
for language,standard,compiler in [('c','c11',os.environ.get('CC','cc')),('c++','c++17',os.environ.get('CXX','c++'))]:
 for extended in (False,True):
  label=('cpp' if language=='c++' else 'c')+('-alarm' if extended else '-baseline')
  flags=['-x',language,'-std='+standard,'-O0','-g0','-pipe','-Wall','-Wextra','-Werror','-pedantic',
         '-I'+str(runtime/'sdk/app'),'-I'+str(ROOT/'sdk/app')]
  if extended:flags+=['-DTEST_ALARM_CONTRACT']
  if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie']
  deps=run([compiler,*flags,'-MM',str(source)],label+'-dependencies')
  paths=[Path(v).resolve() for v in shlex.split(deps.replace('\\\n',' ').split(':',1)[1])]
  for path in paths:
   digest=hashlib.sha256(path.read_bytes()).hexdigest()
   assert str(path) not in inputs or inputs[str(path)]==digest,'Input changed between compiles'
   inputs[str(path)]=digest
  binary=out/label
  run([compiler,*flags,str(source),'-o',str(binary)],label+'-build')
  layouts[label]=run([str(binary)],label+'-run')
  binaries[label]=hashlib.sha256(binary.read_bytes()).hexdigest()
assert len(set(layouts.values()))==1,'Original and extended C/C++ layouts differ'
assert inputs=={path:hashlib.sha256(Path(path).read_bytes()).hexdigest() for path in inputs},'Inputs changed'
(out/'receipt.json').write_text(json.dumps({'passed':True,'cases':4,'sanitized':a.sanitize,
 'runtime_revision':'0052d1fc42e9cfc82d626c5a247d460919c87935','shell_git_blob':'26356c2b0a836c9d73efda058c80a71c3233fc38',
 'inputs':inputs,'commands':commands,'binary_sha256':binaries,'layouts':layouts,
 'scope':'Header constants and host-compiler C11/C++17 layout compatibility only. No Runtime/old-host/device behavior executed.'},indent=2)+'\n')
print('4 strict header/layout cases PASS')
