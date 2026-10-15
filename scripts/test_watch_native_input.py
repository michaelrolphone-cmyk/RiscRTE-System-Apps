#!/usr/bin/env python3
"""Native coordinates preserve the existing ordered Quick Controls routing."""
from pathlib import Path
import os,subprocess
root=Path(__file__).resolve().parents[1];out=root/'build/watch-native-input';out.mkdir(parents=True,exist_ok=True)
for sanitize in (False,True):
 for width,height in ((240,240),(410,502)):
  for rotation in (0,180):
   exe=out/f'{width}-{height}-{rotation}-{int(sanitize)}'
   cmd=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_NOVA_UI','-DPORTABLE_WATCH_NATIVE_HEIGHT',f'-DPORTABLE_TOUCH_ROTATION={rotation}',f'-DSETTINGS_FIXTURE_WIDTH={width}',f'-DSETTINGS_FIXTURE_HEIGHT={height}','-I'+str(root/'lib/PortableApps/include'),'-I'+str(root/'lib/NativeApps/include')]
   if sanitize:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
   cmd += [str(root/'Apps/settings.c'),str(root/'test/native_apps/watch_native_input_test.c'),*[str(root/'lib/PortableApps/src'/(x+'.c')) for x in ('quick_actions','quick_render','quick_session')],'-o',str(exe)]
   subprocess.run(cmd,check=True);subprocess.run([str(exe)],check=True,timeout=60)
print('Native input: 8 native/rotation/sanitizer cases PASS')
