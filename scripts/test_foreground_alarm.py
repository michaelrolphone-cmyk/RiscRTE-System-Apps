#!/usr/bin/env python3
"""Owning foreground-alarm bridge with real tagged client and modeled service."""
import argparse, hashlib, json, os, shutil, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--runtime',type=Path,required=True)
p.add_argument('--utilities',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
inc=out/'include';shutil.copytree(ROOT/'lib/PortableApps/include',inc,dirs_exist_ok=True)
for base in (a.runtime/'sdk/app',a.utilities/'lib/Alarm/include'):
 for h in base.glob('*.h'):shutil.copyfile(h,inc/h.name)
runs=[];deps=set()
for lease,san in ((False,False),(False,True),(True,False),(True,True)):
 exe=out/(('lease-' if lease else '')+('sanitized' if san else 'normal'));dep=exe.with_suffix('.d')
 flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-MMD','-MF',str(dep)]
 if lease:flags+=['-DPORTABLE_WIFI_SERVICE_LEASE']
 if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 cmd=[os.environ.get('CC','cc'),*flags,'-I'+str(inc),str(ROOT/'test/native_apps/foreground_alarm_test.c'),'-o',str(exe)]
 subprocess.run(cmd,check=True)
 run=subprocess.run([exe],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
 (out/(exe.name+'.log')).write_text(run.stdout);print(run.stdout,end='',flush=True);run.check_returncode()
 runs.append({'lease':lease,'sanitized':san,'command':cmd,'result':'pass'})
 deps.update(Path(x) for x in dep.read_text().replace('\\\n',' ').split(':',1)[1].split())
deps.add(Path(__file__))
(out/'evidence.json').write_text(json.dumps({'hardware_verified':False,'scope':'Production foreground alarm bridge and tagged client; modeled service/runtime; not hardware sleep integration','runs':runs,'source_revisions':{str(x.resolve()):subprocess.check_output(['git','-C',str(x),'rev-parse','HEAD'],text=True).strip() for x in (ROOT,a.runtime,a.utilities)},'dependency_sha256':{str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in sorted(deps)}},indent=2)+'\n')
