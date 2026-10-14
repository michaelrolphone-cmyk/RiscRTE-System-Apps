#!/usr/bin/env python3
"""Deployed RGB timer/Quick/automatic integration with copied atomic storage."""
import argparse,json,os,subprocess,hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
runs=[];commands=[]
for rotation in (0,180):
 for native in (False,True):
  flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_NOVA_UI','-DPORTABLE_TOUCH_ROTATION='+str(rotation),'-DPORTABLE_LOW_BATTERY','-DPORTABLE_QUICK_RADIOS','-DPORTABLE_QUICK_ATOMIC_PREFERENCES','-DPORTABLE_SLEEP_SETTINGS','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  if native:flags+=['-DPORTABLE_WATCH_NATIVE_HEIGHT','-DSETTINGS_FIXTURE_WIDTH=410','-DSETTINGS_FIXTURE_HEIGHT=502']
  for fixture,cases,extra in [('rgb_preferences_test',range(4),[]),('quick_adapter_test',range(13),['-DRGB_ATOMIC_FIXTURE','-DPORTABLE_SETTINGS_ATOMIC_EXPERIMENTAL'])]:
   exe=a.output/f'{fixture}-{rotation}-{int(native)}'
   sources=[root/'Apps/settings.c',root/'test/native_apps'/(fixture+'.c'),*[root/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_session.c','quick_radios.c','quick_render.c')]]
   cmd=['cc',*flags,*extra,*['-I'+str(root/q) for q in ('lib/PortableApps/include','lib/NativeApps/include')],*map(str,sources),'-o',str(exe)]
   commands.append(cmd);subprocess.run(cmd,check=True)
   for case in cases:
    r=subprocess.run([str(exe),str(case)],capture_output=True,text=True,timeout=40,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
    if r.returncode:raise RuntimeError(f'{fixture=} {rotation=} {native=} {case=}\n'+r.stdout+r.stderr)
    runs.append({'fixture':fixture,'rotation':rotation,'native':native,'case':case,'result':r.stdout.strip()})
   print(f'{fixture} rotation={rotation} native={native}: {len(cases)} ASan/UBSan cases PASS',flush=True)
files=[f for d in ('lib/PortableApps','test/native_apps','Apps/settings.c') for f in ((root/d).rglob('*') if (root/d).is_dir() else [root/d]) if f.is_file()]
(a.output/'receipt.json').write_text(json.dumps({'hardware_tested':False,'runs':runs,'commands':commands,'source_sha256':{str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(files)}},indent=2)+'\n')
