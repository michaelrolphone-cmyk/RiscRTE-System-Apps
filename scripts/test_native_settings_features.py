#!/usr/bin/env python3
"""Native Settings:52 bounded feature cases plus2 RTC/alarm boundary cases."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--system',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args();root=args.system.resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
native=['-DPORTABLE_SETTINGS_NATIVE_UI','-DPORTABLE_NATIVE_CANVAS','-DPORTABLE_NOVA_UI']
motion=['-DPORTABLE_PAPER_TRANSITIONS']
base=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(root/'lib/PortableApps/include'),'-I'+str(root/'lib/NativeApps/include')]
quick=['lib/PortableApps/src/'+n for n in ('quick_actions.c','quick_render.c','quick_session.c','quick_radios.c')]
profiles=[
 ('clock-change',native,['Apps/settings.c','test/native_apps/settings_clock_change_test.c'],[[]]),
 ('quick-core',native+motion,['test/native_apps/quick_core_test.c',*quick[:2]],[[]]),
 ('quick-session',native+motion,['test/native_apps/quick_session_test.c',*quick[:3]],[[]]),
 ('quick-radios',native+motion,['test/native_apps/quick_radio_test.c',quick[0],quick[3]],[[]]),
 ('quick-native',native+motion+['-DPORTABLE_RASTER_SNAPSHOT'],['test/native_apps/quick_native_test.c',*quick[:2]],[[]]),
 ('quick-adapter',native+motion+['-DPORTABLE_QUICK_RADIOS','-DPORTABLE_SLEEP_SETTINGS','-DPORTABLE_ALARM_SETTINGS','-DPORTABLE_SETTINGS_CLOCK_CHANGE_CONTROL'],['Apps/settings.c','test/native_apps/quick_adapter_test.c',*quick],[[str(n)] for n in range(13)]),
 ('sleep-policy',native,['test/native_apps/portable_sleep_policy_test.c'],[[]]),
 ('native-idle',native,['Apps/settings.c','test/native_apps/portable_idle_sleep_test.c'],[[str(n)] for n in (0,1,2,3,4,7)]),
 ('native-retained',native,['Apps/settings.c','test/native_apps/portable_retained_sleep_test.c'],[[],['time-format']]),
]
rows=[]
for name,flags,sources,cases in profiles:
 for san in (False,True):
  label=f'{name}-{int(san)}';target=out/label
  extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
  cmd=[*base,*flags,*extra,*[str(root/n) for n in sources],'-o',str(target)]
  with (out/(label+'-compile.log')).open('w') as log:
   result=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=120)
  if result.returncode:
   print('COMPILE FAILED',label,flush=True);rows.append({'fixture':name,'sanitized':san,'compile_command':cmd,'result':'compile-failed'});continue
  for case in cases:
   result=subprocess.run([str(target),*case],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=30,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
   (out/(label+'-'+('-'.join(case) or 'main')+'.log')).write_text(result.stdout)
   row={'fixture':name,'sanitized':san,'case':case,'result':'passed' if result.returncode==0 else 'failed','returncode':result.returncode,'build_defines':flags,'compile_command':cmd,'source_sha256':{p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in sources}}
   rows.append(row);print(label,case,row['result'],result.stdout.strip(),flush=True)
  target.unlink()
(out/'receipt.json').write_text(json.dumps({'runs':rows,'hardware_verified':False,'boundary':'Existing host fixtures built with explicit native Settings flags; no fixture coordinates changed. Native idle nested-coordinate cases 5 and 6 excluded; separately sized parent controller coverage owns native selectors.'},indent=2)+'\n')
print('RESULT',len(rows),'runs',sum(r['result']=='passed' for r in rows),'passed',flush=True)
if any(r['result']!='passed' for r in rows):raise SystemExit(1)
