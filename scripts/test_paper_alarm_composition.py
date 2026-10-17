#!/usr/bin/env python3
"""Provider-only Clock/Settings paper alarm flag composition; no hardware."""
import argparse,hashlib,json,os,shlex,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--normal-only',action='store_true');a=p.parse_args()
O=a.output.resolve();O.mkdir(parents=True,exist_ok=True);rows=[]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
for sanitized in ([False] if a.normal_only else [False,True]):
 for suite in ('clock','settings'):
  for height in (False,True):
   for native in (False,True):
    name=f'{suite}-height{int(height)}-native{int(native)}-san{int(sanitized)}';exe=O/'test-binary';frames=O/name;frames.mkdir(exist_ok=True)
    flags=['-DPORTABLE_ALARM_CLIENT','-DPORTABLE_NOVA_UI','-DPORTABLE_NATIVE_CANVAS']
    flags+=['-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_RTC_WALL_TIME'] if suite=='clock' else ['-DPORTABLE_SETTINGS_APP','-DPORTABLE_SETTINGS_NATIVE_UI']
    if height:flags+=['-DPORTABLE_WATCH_NATIVE_HEIGHT']
    if native:flags+=['-DPORTABLE_ALARM_NATIVE']
    if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
    sources=['Apps/paper_clock.c','lib/PortableApps/src/adapter.c','test/native_apps/paper_clock_test.c'] if suite=='clock' else ['Apps/settings.c','lib/PortableApps/src/adapter.c','test/native_apps/paper_alarm_composition_test.c']
    cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),*[str(ROOT/s) for s in sources]]
    dep=subprocess.run([cmd[0],'-MM',*cmd[1:]],text=True,capture_output=True,check=True);(O/(name+'.d')).write_text(dep.stdout)
    dependencies={}
    for path in shlex.split(dep.stdout.replace('\\\n',' ')):
     if path.endswith(':'):continue
     f=Path(os.path.normpath(path));dependencies[str(f.relative_to(ROOT))]=sha(f)
    row={'name':name,'suite':suite,'height':height,'native':native,'sanitized':sanitized,'command':cmd,'dependencies':dependencies}
    try:
     build=subprocess.run([*cmd,'-o',str(exe)],text=True,capture_output=True,timeout=120);(O/(name+'-build.log')).write_text(build.stdout+build.stderr);build.check_returncode();row['binary_sha256']=sha(exe)
     env={**os.environ,'WITNESS_FRAMES':str(frames),'PAPER_FRAME':str(frames/'first.pbm'),'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'}
     run=subprocess.run([str(exe),*(['13'] if suite=='clock' else [])],env=env,text=True,capture_output=True,timeout=20);(O/(name+'.log')).write_text(run.stdout+run.stderr);row.update(returncode=run.returncode,output=run.stdout+run.stderr,captures={f.name:sha(f) for f in frames.glob('*.pbm')})
     rows.append(row);(O/'receipt.json').write_text(json.dumps({'hardware':False,'rows':rows},indent=2)+'\n');print(name,run.returncode,run.stdout+run.stderr,flush=True);run.check_returncode()
    finally:
     if exe.exists():exe.unlink()
# Every selector/sanitizer composition keeps identical provider pixels.
EXPECTED_SETTINGS_FRAMES={'frame-1.pbm': '349e29dabb0d92d9957bc305074b8452bdf55282955505d5f9c246ae1e894ab8', 'frame-2.pbm': '76973f2841b34d15747db9c94725329447fc8ac79a5450e724772354c6b60b88', 'frame-3.pbm': '349e29dabb0d92d9957bc305074b8452bdf55282955505d5f9c246ae1e894ab8'}
for row in rows:
 if row['suite']=='settings':assert {k:v for k,v in row['captures'].items() if k.startswith('frame-')}==EXPECTED_SETTINGS_FRAMES,(row['name'],'legacy frame bytes changed')
for suite in ('clock','settings'):
 selected=[r for r in rows if r['suite']==suite]
 assert all(r['captures']==selected[0]['captures'] for r in selected),(suite,'capture mismatch')
print(f'PASS {len(rows)} paper alarm compositions; exact ACK/restoration and equivalent provider bytes')
