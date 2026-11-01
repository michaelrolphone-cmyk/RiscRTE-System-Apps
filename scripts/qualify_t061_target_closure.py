#!/usr/bin/env python3
"""Capture actual per-TU dependency bytes while repeating the selected Home target."""
import pathlib,json,subprocess,hashlib,runpy,sys,os,shlex,argparse
p=argparse.ArgumentParser();p.add_argument('--target',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();R=pathlib.Path(__file__).resolve().parents[1];T=a.target.resolve();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True);rows=[];inputs={};real=subprocess.run
command=json.loads((T/'command.json').read_text())['command'];command[-1]=str(O/'home');os.environ['NATIVE_APP_CC']=str(T/'ccbin/xtensa-esp32s3-elf-gcc')
def run(cmd,*args,**kwargs):
 if isinstance(cmd,list) and str(cmd[0]).endswith('gcc') and '-shared' in cmd:
  cmd=list(map(str,cmd));sources=[x for x in cmd if x.endswith('.c')];flags=[];skip=False
  for x in cmd[1:]:
   if skip:skip=False;continue
   if x=='-o':skip=True;continue
   if x.endswith('.c') or x in ('-shared','-lgcc') or x.startswith('-Wl,'):continue
   flags.append(x)
  for i,s in enumerate(sources):
   dep=O/(str(i)+'.d');probe=[cmd[0],*flags,'-MM','-MF',str(dep),s];real(probe,check=True)
   closure={}
   for name in shlex.split(dep.read_text().replace('\\\n',' ').split(':',1)[1]):
    p=pathlib.Path(name).resolve();h=hashlib.sha256(p.read_bytes()).hexdigest();closure[str(p)]=h
    assert str(p) not in inputs or inputs[str(p)]==h;inputs[str(p)]=h
   rows.append({'translation_unit':s,'probe':probe,'dependencies':closure})
  (O/'target-command.json').write_text(json.dumps(cmd,indent=2))
 return real(cmd,*args,**kwargs)
subprocess.run=run;sys.path.insert(0,str(R/'scripts'));sys.argv=command[1:];runpy.run_path(command[1],run_name='__main__')
assert all(hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()==h for p,h in inputs.items())
elf=O/'home/default.elf';assert elf.read_bytes()==(T/'home/default.elf').read_bytes()
(O/'receipt.json').write_text(json.dumps({'translation_units':rows,'inputs':inputs,'compiler_wrapper_extra_define':'PORTABLE_ALARM_BUSY_DEFER','elf_sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),'matches_first_and_repeat':True},indent=2))
