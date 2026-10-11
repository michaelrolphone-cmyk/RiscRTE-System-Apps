#!/usr/bin/env python3
"""Exercise opt-in Contexts against the actual X4 shared adapter."""
import argparse, os, shutil, subprocess, tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--sdk-include',type=Path,required=True,help='Existing canonical native Runtime/tagged alarm include directory')
p.add_argument('--app-data-sdk',type=Path,required=True,help='Canonical Runtime app SDK with RiscAppDataV1.h')
a=p.parse_args();out=r/'build/contexts-x4';out.mkdir(parents=True,exist_ok=True)
cases='missing-empty dirty-refused missing-size empty-success invalid-api normal busy-state mask synchronous auto-sleep sleep sleep-retained capture-raster capture-input capture-present pause enable-pause pending-capture app-data app-data-refused storage release'.split()
with tempfile.TemporaryDirectory(prefix='contexts-x4-') as t:
 inc=Path(t)/'include';shutil.copytree(r/'lib/PortableApps/include',inc);shutil.copytree(r/'lib/PortableApps/time',inc.parent/'time')
 for name in ['RiscRuntimeV1.h','RiscRealtimeV1.h','AlarmServiceV1.h','AlarmServiceV2.h']:shutil.copyfile(a.sdk_include/name,inc/name)
 shutil.copyfile(a.app_data_sdk/'RiscAppDataV1.h',inc/'RiscAppDataV1.h')
 flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_CONTEXTS_CLIENT','-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DALARM_SERVICE_TAGGED_V2','-DTEST_NATIVE_TOOLBAR_QUICK','-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_X4_IDLE_POLICY','-DPORTABLE_PAPER_PREFERENCES','-I'+str(inc),'-I'+str(r/'lib/NativeApps/include')]
 for broadcast,san,editor in [(False,False,False),(False,True,False),(True,False,False),(True,True,False),(False,False,True),(False,True,True)]:
  exe=out/(('editor-' if editor else '')+('broadcast-' if broadcast else '')+('sanitized' if san else 'normal'))
  extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
  if editor:extra+=['-DPORTABLE_CONTEXTS_EDITOR']
  if broadcast:extra+=['-DPORTABLE_BLE_BROADCAST','-DPORTABLE_BLE_BROADCAST_DEFAULT_OFF']
  sources=[r/'test/native_apps/context_x4_adapter_test.c',*[r/'lib/PortableApps/src'/n for n in ['quick_actions.c','quick_session.c','quick_render.c']]]
  subprocess.run([os.environ.get('CC','cc'),*flags,*extra,*map(str,sources),'-Wl,--wrap=free','-o',str(exe)],check=True)
  for case in (c for c in cases if not editor or c!='mask'):subprocess.run([exe,case],check=True,timeout=20,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
