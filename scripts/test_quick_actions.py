#!/usr/bin/env python3
"""Exercise the real shared adapter dispatch/session with deterministic providers."""
from pathlib import Path
import subprocess,os
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/quick-actions';OUT.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0')
core=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-pedantic',
      '-I'+str(ROOT/'lib/PortableApps/include'),str(ROOT/'lib/PortableApps/src/quick_actions.c'),str(ROOT/'lib/PortableApps/src/quick_render.c')]
for san in (False,True):
 target=OUT/f'core-{int(san)}'
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
 subprocess.run(core+flags+[str(ROOT/'test/native_apps/quick_core_test.c'),'-o',str(target)],check=True,timeout=120)
 subprocess.run([str(target)],check=True,timeout=20,env=env)
for san in (False,True):
 target=OUT/f'session-{int(san)}'
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
 subprocess.run(core+flags+[str(ROOT/'lib/PortableApps/src/quick_session.c'),str(ROOT/'test/native_apps/quick_session_test.c'),'-o',str(target)],check=True,timeout=120)
 subprocess.run([str(target)],check=True,timeout=20,env=env)
frame=OUT/'render-frame'
subprocess.run(core+[str(ROOT/'test/native_apps/quick_render_frame.c'),'-o',str(frame)],check=True,timeout=120)
for name,args in [('open',['240']),('partial',['160']),('silent',['240','silent']),('torch',['240','silent','torch'])]:
 subprocess.run([str(frame),str(OUT/(name+'.ppm'))]+args,check=True,timeout=20)
for radios in (False,True):
 for san in (False,True):
  for rotation in (0,180):
   target=OUT/f'adapter-{rotation}-{int(san)}-{int(radios)}'
   cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_NOVA_UI',f'-DPORTABLE_TOUCH_ROTATION={rotation}',
        '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include')]
   if radios:cmd+=['-DPORTABLE_QUICK_RADIOS']
   if san:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
   cmd+=[str(ROOT/'Apps/settings.c'),str(ROOT/'test/native_apps/quick_adapter_test.c')]
   cmd += [str(ROOT/'lib/PortableApps/src'/name) for name in ['quick_actions.c','quick_render.c','quick_session.c']+(['quick_radios.c'] if radios else [])]
   subprocess.run(cmd+['-o',str(target)],check=True,timeout=120)
   for case in range(13):
    frames=OUT/f'frames-{rotation}-{int(san)}-{int(radios)}-{case}';frames.mkdir(exist_ok=True)
    subprocess.run([str(target),str(case),str(frames)],check=True,timeout=20,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
try:
 from PIL import Image
 for ppm in list(OUT.glob('frames-*/*.ppm'))+list(OUT.glob('*.ppm')):Image.open(ppm).save(ppm.with_suffix('.png'))
except ImportError:pass
print('Quick actions: 2 pure-core + 2 session + 104 real-adapter normal/sanitizer/rotation executions passed')
