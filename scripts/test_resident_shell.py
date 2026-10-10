#!/usr/bin/env python3
"""Load separate real host/client ELF modules with explicit lifecycle providers."""
import argparse,os,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime-sdk',type=Path,required=True);p.add_argument('--display-sdk',type=Path,required=True);p.add_argument('--sanitize',action='store_true');p.add_argument('--legacy-handoff',action='store_true');p.add_argument('--output-dir',type=Path,default=ROOT/'build/resident-shell');a=p.parse_args()
out=a.output_dir.resolve();inc=out/'include';shutil.copytree(ROOT/'lib/PortableApps/include',inc,dirs_exist_ok=True);shutil.copytree(ROOT/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
for name in ('RiscRuntimeV1.h','RiscResidentShellV1.h'):shutil.copyfile(a.runtime_sdk/name,inc/name)
if (a.runtime_sdk/'RiscFailureEvidenceV1.h').is_file():shutil.copyfile(a.runtime_sdk/'RiscFailureEvidenceV1.h',inc/'RiscFailureEvidenceV1.h')
for name in ('RiscDisplayOutputV1.h','RiscDisplayOutputPowerV1.h','RiscDisplayOutputMetricsV1.h','RiscDisplayOutputSnapshotV1.h'):shutil.copyfile(a.display_sdk/name,inc/name)
base=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(inc),'-I'+str(ROOT/'lib/NativeApps/include')]
if a.sanitize:base+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
flags=['-DPORTABLE_PAPER_PREFERENCES','-DPORTABLE_ALARM_TERMINAL_RETENTION','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_HOME_APP="default.elf"']
for role in ('host','client'):
 sources=[ROOT/'test/native_apps/resident_shell_app.c',ROOT/'lib/PortableApps/src/adapter.c']
 extra=['-DPORTABLE_RESIDENT_SHELL_'+role.upper()]
 if role=='client':extra+=['-DPORTABLE_APP_LAUNCH_GUARD','-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_CROWN_SLEEP_LOCAL']
 if role=='host':
  if a.legacy_handoff:extra+=['-DPORTABLE_RESIDENT_LEGACY_HANDOFF']
  extra+=['-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_QUICK_USB_TRANSFER','-DPORTABLE_PAPER_TRANSITIONS']
  sources += [ROOT/'lib/PortableApps/src'/name for name in ('quick_actions.c','quick_session.c','quick_render.c')]
 subprocess.run(base+['-fPIC','-shared','-Wl,-Bsymbolic',*flags,*extra,*map(str,sources),'-o',str(out/(role+'.elf'))],check=True)
 symbols=subprocess.check_output(['nm',str(out/(role+'.elf'))],text=True)
 assert (' pqa_render' in symbols)==(role=='host')
 assert (b'QUICK ACTIONS' in (out/(role+'.elf')).read_bytes())==(role=='host')
 assert ' risc_resident_app_descriptor_v1' in symbols
binary=out/'resident-test'
subprocess.run(base+['-rdynamic',*(['-no-pie'] if a.sanitize else []),*(['-DTEST_RESIDENT_LEGACY_HANDOFF'] if a.legacy_handoff else []),str(ROOT/'test/native_apps/resident_shell_test.c'),'-ldl','-o',str(binary)],check=True)
for mode in [*range(20 if a.legacy_handoff else 13),20]:subprocess.run([str(binary),str(out),str(mode)],check=True,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
print('Separate ELF host/client: repeated overlays, exact image/model restore, focus, Home, queued USB, refusal, retention and no duplicate renderer PASS')

try:
 from PIL import Image
 for source in out.glob('quick-*.pbm'):Image.open(source).save(source.with_suffix('.png'))
except ImportError:pass
