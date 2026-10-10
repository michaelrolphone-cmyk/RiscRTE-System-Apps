#!/usr/bin/env python3
"""Production host and product helper across native sleep/refusal/retention."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--runtime',type=Path,required=True);p.add_argument('--product',type=Path,required=True)
p.add_argument('--driver-sdk',type=Path,required=True);p.add_argument('--tone-sdk',type=Path);p.add_argument('--settled-sdk',type=Path)
p.add_argument('--alarm-sdk',type=Path,required=True);p.add_argument('--output-dir',type=Path,required=True)
p.add_argument('--case',action='append',help='Run only named cases from this matrix')
a=p.parse_args();out=a.output_dir.resolve();inc=out/'include';inc.mkdir(parents=True,exist_ok=True)
shutil.copytree(r/'lib/PortableApps/include',inc,dirs_exist_ok=True)
shutil.copytree(r/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
for directory in (a.runtime/'sdk/app',a.runtime/'sdk/driver',a.driver_sdk,a.product/'minimal/interfaces',a.alarm_sdk,a.product/'minimal/drivers/x4pro_power'):
 for header in directory.glob('*.h'):
  if header.name.startswith(('Risc','AlarmService','X4Power')):shutil.copyfile(header,inc/header.name)
for name in ('RiscRuntimeV1.h','RiscResidentShellV1.h','RiscFailureEvidenceV1.h'):
 shutil.copyfile(a.runtime/'sdk/app'/name,inc/name)
flags=['-D'+n for n in ('PORTABLE_STAGE_LOGS','PORTABLE_PAPER_PREFERENCES','PORTABLE_ALARM_TERMINAL_RETENTION','PORTABLE_NATIVE_CUSTODY_FENCE','PORTABLE_ALARM_CLIENT','ALARM_SERVICE_TAGGED_V2','PORTABLE_INPUT_NAVIGATION','PORTABLE_APP_OWNS_TOUCH_CHROME','PORTABLE_RESIDENT_SHELL_HOST','PORTABLE_RESIDENT_POLICY','PORTABLE_APP_SLEEP_LOCAL','PORTABLE_QUICK_ACTIONS','PORTABLE_QUICK_RADIOS','PORTABLE_PAPER_TRANSITIONS','PORTABLE_X4_IDLE_POLICY','PORTABLE_LOW_BATTERY')]
if a.tone_sdk:
 shutil.copyfile(a.tone_sdk/'RiscDisplayOutputFrontlightV1.h',inc/'RiscDisplayOutputFrontlightV1.h');flags+=['-DPORTABLE_FRONTLIGHT_TONE']
if a.settled_sdk:
 for name in ('RiscDisplayOutputFrontlightV1.h','RiscDisplayOutputSettledV1.h'):shutil.copyfile(a.settled_sdk/name,inc/name)
 flags+=['-DPORTABLE_DISPLAY_SETTLED']
sources=[r/'test/native_apps/resident_idle_app.c',r/'test/native_apps/resident_idle_adapter.c',*[r/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_session.c','quick_render.c','quick_radios.c')],a.product/'minimal/apps/portable_idle_sleep.c']
cases='ok slow repeat flip-on flip-off known-failed known-retained native-refused panel-refused cancel-prepared alarm-cancel snapshot-refused acquire-retained wifi-retained alarm-retained panel-retained native-retained frame-retained submit-retained present-retained restore-frame-retained restore-submit-retained restore-present-retained'.split()
if a.tone_sdk:cases+=['tone-ok','tone-repeat','tone-get-retained','tone-set-retained']
if a.settled_sdk:cases+='settled-request-retained settled-request-refused settled-ok settled-short settled-repeat settled-cancel settled-late-touch settled-late-home settled-home settled-home-held settled-refused settled-retained settled-newer settled-timeout settled-stalled settled-backward settled-touch-retained settled-close-retained settled-malformed settled-cancel-prepared'.split()
if a.case:
 if any(case not in cases for case in a.case):p.error('Unknown or unselected case')
 cases=a.case
inputs=set(sources)
for directory in (r/'lib/PortableApps',r/'lib/NativeApps/include',r/'test/native_apps',inc,out/'time',a.product/'minimal/drivers/x4pro_power'):
 inputs.update(path for path in directory.rglob('*') if path.is_file())
inputs.update(path for path in (r/'Apps').glob('*') if path.is_file())
inputs.add(Path(__file__).resolve())
source_inputs={str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(inputs)}
runs=[]
for sanitized in (False,True):
 build=out/('sanitized' if sanitized else 'normal');build.mkdir(exist_ok=True)
 san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'] if sanitized else []
 base=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*san,'-I'+str(inc),'-I'+str(r/'lib/NativeApps/include')]
 subprocess.run([*base,*flags,'-fPIC','-shared','-Wl,-Bsymbolic',*map(str,sources),'-o',str(build/'host.elf')],check=True)
 binary=build/'test';subprocess.run([*base,'-DPORTABLE_ALARM_CLIENT','-DALARM_SERVICE_TAGGED_V2','-DPORTABLE_X4_IDLE_POLICY',*(['-DPORTABLE_FRONTLIGHT_TONE'] if a.tone_sdk else []),*(['-DPORTABLE_DISPLAY_SETTLED'] if a.settled_sdk else []),'-rdynamic',*(['-no-pie'] if sanitized else []),str(r/'test/native_apps/resident_idle_test.c'),'-ldl','-o',str(binary)],check=True)
 for case in cases:
  result=subprocess.run([binary,build,case],check=True,timeout=20,text=True,stdout=subprocess.PIPE,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
  print(result.stdout.strip(),flush=True);runs.append({'case':case,'sanitized':sanitized,'result':result.stdout.strip()})
assert all(hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest for path,digest in source_inputs.items()),'Test input changed during matrix'
receipt={'hardware_tested':False,'runtime_mocked':True,'scope':'Production host policy, overlay, adapter and product idle helper; synthetic native and alarm providers. Separate product suite uses actual alarm service.','retained_image_boundary':'Once begin draws Sleeping, any later retained outcome leaves the physical image unchanged. No repaint or provider I/O follows terminal custody.','runs':runs,'build_defines':flags,'source_and_staged_sdk_sha256':source_inputs}
(out/'evidence.json').write_text(json.dumps(receipt,indent=2)+'\n')
from PIL import Image
for source in out.glob('*/sleeping.pbm'):Image.open(source).save(source.with_suffix('.png'))
