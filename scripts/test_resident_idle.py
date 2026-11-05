#!/usr/bin/env python3
"""Production host and product helper across native sleep/refusal/retention."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--runtime',type=Path,required=True);p.add_argument('--product',type=Path,required=True)
p.add_argument('--driver-sdk',type=Path,required=True);p.add_argument('--tone-sdk',type=Path);p.add_argument('--settled-sdk',type=Path)
p.add_argument('--alarm-sdk',type=Path,required=True);p.add_argument('--output-dir',type=Path,required=True)
p.add_argument('--legacy-policy-root',type=Path,help='Compile the frozen resident policy and shell includes for an old-host compatibility witness')
p.add_argument('--case',action='append',help='Run only named cases from this matrix')
p.add_argument('--async-display',action='store_true',help='Use deployed sliced rendering and delayed frame completion')
a=p.parse_args();out=a.output_dir.resolve();inc=out/'include';inc.mkdir(parents=True,exist_ok=True)
shutil.copytree(r/'lib/PortableApps/include',inc,dirs_exist_ok=True)
shutil.copytree(r/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
for directory in (a.runtime/'sdk/app',a.runtime/'sdk/driver',a.driver_sdk,a.product/'minimal/interfaces',a.alarm_sdk,a.product/'minimal/drivers/x4pro_power'):
 for header in directory.glob('*.h'):
  if header.name.startswith(('Risc','AlarmService','X4Power')):shutil.copyfile(header,inc/header.name)
for name in ('RiscRuntimeV1.h','RiscResidentShellV1.h','RiscFailureEvidenceV1.h'):
 shutil.copyfile(a.runtime/'sdk/app'/name,inc/name)
flags=['-D'+n for n in ('PORTABLE_STAGE_LOGS','PORTABLE_PAPER_PREFERENCES','PORTABLE_ALARM_TERMINAL_RETENTION','PORTABLE_NATIVE_CUSTODY_FENCE','PORTABLE_ALARM_CLIENT','ALARM_SERVICE_TAGGED_V2','PORTABLE_INPUT_NAVIGATION','PORTABLE_APP_OWNS_TOUCH_CHROME','PORTABLE_RESIDENT_SHELL_HOST','PORTABLE_RESIDENT_POLICY','PORTABLE_APP_SLEEP_LOCAL','PORTABLE_QUICK_ACTIONS','PORTABLE_QUICK_RADIOS','PORTABLE_PAPER_TRANSITIONS','PORTABLE_X4_IDLE_POLICY','PORTABLE_LOW_BATTERY')]
if a.async_display:flags+=['-DPORTABLE_RASTER_SNAPSHOT']
if a.tone_sdk:
 shutil.copyfile(a.tone_sdk/'RiscDisplayOutputFrontlightV1.h',inc/'RiscDisplayOutputFrontlightV1.h');flags+=['-DPORTABLE_FRONTLIGHT_TONE']
if a.settled_sdk:
 for name in ('RiscDisplayOutputFrontlightV1.h','RiscDisplayOutputSettledV1.h'):shutil.copyfile(a.settled_sdk/name,inc/name)
 flags+=['-DPORTABLE_DISPLAY_SETTLED']
sources=[r/'test/native_apps/resident_idle_app.c',r/'test/native_apps/resident_idle_adapter.c',*[r/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_session.c','quick_render.c','quick_radios.c')],a.product/'minimal/apps/portable_idle_sleep.c']
if a.legacy_policy_root:
 owner=out/'legacy-owner';staged=owner/'lib/PortableApps/src';shutil.copytree(r/'lib/PortableApps/src',staged,dirs_exist_ok=True)
 if not (owner/'Apps').exists():(owner/'Apps').symlink_to(r/'Apps',target_is_directory=True)
 for folder in (r/'lib/PortableApps').iterdir():
  if folder.is_dir() and folder.name!='src' and not (staged.parent/folder.name).exists():(staged.parent/folder.name).symlink_to(folder,target_is_directory=True)
 for name in ('resident_policy.inc','resident_shell.inc'):
  shutil.copyfile(a.legacy_policy_root/'lib/PortableApps/src'/name,staged/name)
 wrapper=out/'legacy-owner-adapter.c'
 wrapper.write_text((r/'test/native_apps/resident_idle_adapter.c').read_text().replace('../../lib/PortableApps/src/adapter.c',str(staged/'adapter.c')))
 sources[1]=wrapper
frozen_scene=r/'sdk/app/RiscSceneResidentV1.h'
assert hashlib.sha256(frozen_scene.read_bytes()).hexdigest()=='145ef678f3330e4fab85430395b5799dde8dd05ce056f7b2f02ad5a9f58d0414','7fb13b09 scene resident helper changed'
shutil.copyfile(frozen_scene,inc/'review_frozen_scene_resident.h')
for name in ('RiscSceneV1.h','RiscSceneLifecycleV1.h'):shutil.copyfile(r/'sdk/app'/name,inc/name)
cases='ok slow repeat flip-on flip-off known-failed known-retained native-refused panel-refused cancel-prepared alarm-cancel snapshot-refused acquire-retained wifi-retained alarm-retained panel-retained native-retained frame-retained submit-retained present-retained restore-frame-retained restore-submit-retained restore-present-retained'.split()
cases+='continuity-low-battery continuity-deadline continuity-preferences continuity-inhibit continuity-full-detach continuity-old-scene-client continuity-legacy-host'.split()
if a.tone_sdk:cases+=['tone-ok','tone-repeat','tone-get-retained','tone-set-retained']
if a.settled_sdk:cases+='settled-request-retained settled-request-refused settled-ok settled-short settled-repeat settled-cancel settled-late-touch settled-late-home settled-home settled-home-held settled-refused settled-retained settled-newer settled-timeout settled-stalled settled-backward settled-touch-retained settled-close-retained settled-malformed settled-cancel-prepared'.split()
if not a.legacy_policy_root:cases.remove('continuity-legacy-host')
else:cases=['continuity-legacy-host']
if a.case:
 if any(case not in cases for case in a.case):p.error('Unknown or unselected case')
 cases=a.case
inputs=set(sources)
if a.legacy_policy_root:inputs.update(path for path in staged.rglob('*') if path.is_file())
for directory in (r/'lib/PortableApps',r/'lib/NativeApps/include',r/'test/native_apps',inc,out/'time',a.product/'minimal/drivers/x4pro_power'):
 inputs.update(path for path in directory.rglob('*') if path.is_file())
inputs.update(path for path in (r/'Apps').glob('*') if path.is_file())
inputs.add(Path(__file__).resolve())
source_inputs={str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(inputs)}
runs=[]
for sanitized in (False,True):
 build=out/('sanitized' if sanitized else 'normal');build.mkdir(exist_ok=True)
 san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'] if sanitized else []
 base=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*san,*(['-DIDLE_ASYNC_DISPLAY'] if a.async_display else []),'-I'+str(inc),'-I'+str(r/'lib/NativeApps/include')]
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
