#!/usr/bin/env python3
"""Full linked native Files root and production scene; Runtime/hardware doubled."""
import argparse,subprocess,json,hashlib,os,shlex
from pathlib import Path
from qualify_files_scene import DEFINES
from portable_file_sharing_build import MODULES
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--include',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');p.add_argument('--provider-system',type=Path,default=ROOT);a=p.parse_args();o=a.output.resolve();o.mkdir(parents=True,exist_ok=True)
flags=['-O0','-g1','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-missing-field-initializers','-ffunction-sections','-fdata-sections','-I'+str(a.include.resolve()),'-I'+str(ROOT/'Services/scene_profile'),'-I'+str(ROOT/'Services/text_input')]
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
sources=[ROOT/'test/files_scene/composed_root_test.c',a.provider_system.resolve()/'Services/scene_host/host.c',ROOT/'Apps/files_scene_background.c',*[ROOT/'lib/PortableApps/src'/n for n in ('PortableNetworkSession.c','PortableFileSetup.c','PortableTimeZone.c','PortableTimeZoneCatalog.c')],ROOT/'lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c',*[ROOT/'lib/RemoteFiles'/(n+'.cpp') for n in (*MODULES,'WebDavClient','WebDavRemoteVolume','PortablePeerFiles')]]
rows=[];inputs={};objs=[]
def run(cmd):
 r=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},timeout=120);rows.append(dict(command=list(map(str,cmd)),returncode=r.returncode,output=r.stdout+r.stderr));(o/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');print(r.stdout+r.stderr,end='');r.check_returncode()
for i,src in enumerate(sources):
 cc='c++' if src.suffix=='.cpp' else 'cc';obj=o/(str(i)+'.o');dep=o/(str(i)+'.d');extra=['-D'+d for d in DEFINES] if i==0 else []
 run([cc,'-std=c++17' if cc=='c++' else '-std=c11',*flags,*extra,'-MMD','-MF',dep,'-c',src,'-o',obj]);objs.append(obj)
 for name in shlex.split(dep.read_text().replace('\\\n',' ').split(':',1)[1]):
  f=Path(name).resolve();inputs[str(f)]=hashlib.sha256(f.read_bytes()).hexdigest()
exe=o/'test';run(['c++',*flags,'-Wl,--gc-sections',*objs,'-lm','-o',exe])
for profile in ('watch','paper','gray'):
 for case in ('no-storage','volume-unready','message','contexts','contexts-message','message-held','reentry'):run([exe,case,profile])
assert inputs=={p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in inputs}
(o/'receipt.json').write_text(json.dumps(dict(scope=__doc__,cases=21,sanitized=a.sanitize,compiled_inputs=inputs,hardware_tested=False,actual_runtime=False),indent=2)+'\n')
