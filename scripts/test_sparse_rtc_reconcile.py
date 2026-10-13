#!/usr/bin/env python3
"""Host sparse controller/adapter/X4 sleep regression; no target or hardware claim."""
import argparse, os, shutil, subprocess, tempfile, sys, hashlib
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--source-root',type=Path,help='Optional before-fix production sources; current fixture remains the oracle')
p.add_argument('--runtime',type=Path,required=True)
p.add_argument('--sdk',type=Path,required=True)
p.add_argument('--x4',type=Path,required=True)
p.add_argument('--sanitizer',choices=('none','undefined','address,undefined'),default='none')
a=p.parse_args();root=Path(__file__).resolve().parents[1];runtime=a.runtime.resolve();x4=a.x4.resolve()
# Exact sleep-hook bytes recorded in the delivered .105/.106 Home evidence.
assert hashlib.sha256((x4/'minimal/apps/portable_sleep.c').read_bytes()).hexdigest()=='0858a85e56bb12ef1255b02eeb5fb3618a4e46fd283e0a369f4f3bffefe0462b'
code=a.source_root.resolve() if a.source_root else root
with tempfile.TemporaryDirectory(prefix='sparse-rtc-') as tmp:
 out=Path(tmp);inc=out/'include'
 shutil.copytree(code/'lib/PortableApps/include',inc)
 shutil.copytree(code/'lib/PortableApps/time',out/'time')
 for group in ('app','driver','hardware'):
  for source in (runtime/'sdk'/group).glob('*.h'):shutil.copyfile(source,inc/source.name)
 for name in ('RiscDisplayOutputV1.h','RiscDisplayOutputPowerV1.h','RiscTouchV1.h','RiscTouchPowerV1.h','RiscStorageVolumeV1.h'):shutil.copyfile(a.sdk/name,inc/name)
 flags=['TEST_X4_TIMER_STORAGE','PORTABLE_RASTER_SNAPSHOT','TEST_NATIVE_LANDSCAPE','PORTABLE_DISPLAY_ROTATION=90','PORTABLE_APP_OWNS_TOUCH_CHROME','PORTABLE_RTC_WALL_TIME','PORTABLE_ALARM_CLIENT','PORTABLE_INPUT_NAVIGATION','PORTABLE_APP_SLEEP_LOCAL','PORTABLE_CROWN_SLEEP_LOCAL','PORTABLE_SLEEP_MANUAL_ONLY','PORTABLE_DESK_CLOCK','PORTABLE_DESK_CLOCK_SPARSE_START']
 cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*['-D'+f for f in flags],'-I'+str(inc),'-I'+str(root/'lib/NativeApps/include'),'-I'+str(x4/'minimal/drivers/x4pro_power')]
 # Clang warns on existing unused static-inline helpers in this selected profile.
 if sys.platform=='darwin':cmd+=['-Wno-unused-function']
 if a.sanitizer!='none':cmd+=['-fsanitize='+a.sanitizer,'-fno-sanitize-recover=all','-fno-omit-frame-pointer']
 sources=[code/'Apps/paper_clock.c',*[code/'lib/PortableApps/src'/n for n in ('adapter.c','desk_clock_faces.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c')],x4/'minimal/apps/portable_sleep.c',root/'test/native_apps/sparse_clock_startup_test.c']
 subprocess.run([*cmd,*map(str,sources),'-o',str(out/'test')],check=True)
 for case in ('cold','reset','gpio','terminal','unset','missing-zone','missing-basis','bad-zone','bad-basis','fold','gap','refused','rtc-acquire-retained','rtc-read-retained','panel-retained','resume-retained','native-context','native-release','slow-prepare','cross-minute'):
  subprocess.run([str(out/'test'),case,str(out/(case+'.state'))],check=True,timeout=20,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',**({'CLOCK_ZONE':'America/Denver'} if case in ('fold','gap') else {})))

 # Compare actual production-rendered Home pixels against an accurate native
 # clock on cold and GPIO/deep foreground boots, all six clock faces.
 for case in ('cold','gpio','manual'):
  for face in range(6):
   images=[]
   for drift in (None,'300','-300'):
    image=out/f'{case}-{face}-{drift}.pixels'
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',CLOCK_FACE=str(face),PAPER_FRAME=str(image))
    if drift:env['CLOCK_NATIVE_DRIFT']=drift
    state=out/f'{case}-{face}-{drift}.state'
    subprocess.run([str(out/'test'),case,str(state)],check=True,timeout=20,env=env)
    images.append(state.read_bytes()[-48000:] if case=='manual' else image.read_bytes())
   assert images[0]==images[1]==images[2],(case,face,'RTC corrected render mismatch')
 print('PASS: 20 sparse lifecycle cases and 54 cold/GPIO/deep-entry rendered comparisons; six faces, +/-300s drift')
