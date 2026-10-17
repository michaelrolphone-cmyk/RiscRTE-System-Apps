#!/usr/bin/env python3
"""Qualify pinned generic native navigation/sleep bridges without a target compiler."""
import argparse,hashlib,json,os,shlex,shutil,subprocess
from pathlib import Path
parser=argparse.ArgumentParser(description=__doc__)
for name in ('system','bridges','runtime','profile-source','output'):
 parser.add_argument('--'+name,type=Path,required=True)
args=parser.parse_args();root=args.system.resolve();bridge=args.bridges.resolve();out=args.output.resolve()
out.mkdir(parents=True,exist_ok=True);stage=out/'sdk'
pins={'bridge':'6e82fdcaec0b73ed700d2104cfb4d94f011c19d4',
 'runtime':'48229bd1593a2e24e95bf4537b495ee615990fd3',
 'profile':'b9862e1379813a0914d4926ef59a3feb6fcf656a'}
repos={'bridge':bridge,'runtime':args.runtime.resolve(),'profile':args.profile_source.resolve()}
def git(owner,*args):return subprocess.check_output(['git','-C',str(repos[owner]),*args])
def pinned(owner,path):return git(owner,'show',pins[owner]+':'+path)
if git('bridge','rev-parse','HEAD').decode().strip()!=pins['bridge']:raise ValueError('Select exact generic bridge pin')
shutil.copytree(root/'lib/PortableApps/include',stage/'include',dirs_exist_ok=True)
shutil.copytree(root/'lib/PortableApps/time',stage/'time',dirs_exist_ok=True)
(stage/'runtime-sdk').mkdir(exist_ok=True)
for path in git('runtime','ls-tree','-r','--name-only',pins['runtime'],'sdk/app').decode().splitlines():
 if path.endswith('.h'):(stage/'runtime-sdk'/Path(path).name).write_bytes(pinned('runtime',path))
for name in ('RiscRuntimeV1.h','RiscKeyValueAtomicV1.h'):
 (stage/'include'/name).write_bytes((stage/'runtime-sdk'/name).read_bytes())
(stage/'profile').mkdir(exist_ok=True)
for name in ('watch-alarm-profile.h','AlarmServicePinnedV1.h'):
 (stage/'profile'/name).write_bytes(pinned('profile','profiles/shared-editor-inputs/'+name))
(out/'portable_watch_bridge_test.c').write_bytes(pinned('bridge','tests/portable_watch_bridge_test.c'))
common=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_INPUT_NAVIGATION_LOCAL','-DPORTABLE_NAVIGATION_RETENTION','-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_MOTION_WAKE','-DWATCH_ALARM_SLEEP_RESUME','-DPORTABLE_LOW_BATTERY','-DPORTABLE_SETTINGS_NATIVE_UI','-DPORTABLE_NATIVE_CANVAS','-DPORTABLE_NOVA_UI','-include',str(stage/'profile/watch-alarm-profile.h')]
include=[stage/'include',stage/'profile',stage/'runtime-sdk',bridge/'sdk/app',bridge/'sdk/driver',bridge/'include',bridge/'apps/clock']
sources=[bridge/'apps/clock/portable_navigation.c',bridge/'apps/clock/portable_sleep.c',out/'portable_watch_bridge_test.c']
dependencies=set()
for source in sources:
 raw=subprocess.check_output([*common,*['-I'+str(p) for p in include],'-MM',str(source)],text=True)
 dependencies.update(Path(x).resolve() for x in shlex.split(raw.replace('\\\n',' ').split(':',1)[1]))
for path in dependencies:
 if path.is_relative_to(bridge) and path.read_bytes()!=pinned('bridge',str(path.relative_to(bridge))):
  raise ValueError('Bridge input is not canonical: '+str(path))
cases=list(range(28))+list(range(46,55))+list(range(30,46))+list(range(100,112))
rows=[]
for san in (False,True):
 target=out/('bridge-'+str(int(san)))
 extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
 cmd=[*common,*extra,*['-I'+str(p) for p in include],*map(str,sources),'-Wl,--wrap=malloc','-Wl,--wrap=free','-o',str(target)]
 with (out/('compile-'+str(int(san))+'.log')).open('w') as f:subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=120)
 for width,height in ((240,240),(410,502)):
  for case in cases:
   r=subprocess.run([str(target),str(case),str(width),str(height)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=20,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
   (out/f'case-{int(san)}-{width}x{height}-{case}.log').write_text(r.stdout)
   rows.append({'sanitized':san,'width':width,'height':height,'case':case,'returncode':r.returncode,'result':'passed' if not r.returncode else 'failed'})
   if r.returncode:raise RuntimeError(str(rows[-1])+r.stdout)
  print('bridge',san,width,height,len(cases),'passed',flush=True)
 target.unlink()
(out/'receipt.json').write_text(json.dumps({'bridge_pin':'6e82fdcaec0b73ed700d2104cfb4d94f011c19d4','runs':rows,'source_sha256':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(dependencies)},'external_pins':pins,'build_defines':[a for a in common if a.startswith('-D')],'hardware_verified':False,'sanitizer_leak_check_enabled':False},indent=2)+'\n')
print(len(rows),'bridge fault cases passed',flush=True)
