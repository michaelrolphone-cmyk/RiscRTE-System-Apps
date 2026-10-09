#!/usr/bin/env python3
"""Real paper controllers and adapter: completed-image crossfade qualification."""
import argparse,json,os,shutil,subprocess
from pathlib import Path
from PIL import Image,ImageOps
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--runtime-sdk',type=Path,required=True)
p.add_argument('--display-sdk',type=Path,required=True)
p.add_argument('--snapshot-header',type=Path)
p.add_argument('--output-dir',type=Path,default=ROOT/'build/paper-transition')
a=p.parse_args();out=a.output_dir;out.mkdir(parents=True,exist_ok=True)
include=out/'include';shutil.copytree(ROOT/'lib/PortableApps/include',include,dirs_exist_ok=True);shutil.copytree(ROOT/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
for name in ('RiscRuntimeV1.h','RiscRealtimeV1.h'):shutil.copyfile(a.runtime_sdk/name,include/name)
for name in ('RiscDisplayOutputV1.h','RiscDisplayOutputPowerV1.h','RiscDisplayOutputMetricsV1.h','RiscDisplayOutputSnapshotV1.h'):
 shutil.copyfile(a.snapshot_header if name=='RiscDisplayOutputSnapshotV1.h' and a.snapshot_header else a.display_sdk/name,include/name)
# Actual reviewed native Home raster, rather than invented old/acquire contents.
img=Image.open(ROOT/'docs/evidence/home-parity/home-ready.png').convert('1').rotate(90,expand=True)
assert img.size==(800,480)
source=out/'home.bin';source.write_bytes(ImageOps.invert(img.convert('L')).convert('1').tobytes())
cases='normal slow dropped padded absent malformed refused oom repeat back home launch replace quick quick-slow quick-repeat quick-back quick-home brightness brightness-slow alarm submit-false status-false superseded timeout'.split()
results=[]
for sanitized in (False,True):
 binary=out/('transition-sanitized' if sanitized else 'transition')
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
 pure=out/('blend-sanitized' if sanitized else 'blend')
 subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-I'+str(include),str(ROOT/'test/native_apps/paper_blend_test.c'),'-o',str(pure)],check=True)
 subprocess.run([pure],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
 subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,
  '-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DTEST_NATIVE_TOOLBAR_QUICK','-DPORTABLE_INPUT_NAVIGATION',
  '-DPORTABLE_PAPER_CROSSFADE','-DPORTABLE_PAPER_TRANSITIONS',
  '-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'test/native_apps/paper_transition_test.c'),str(ROOT/'test/native_apps/paper_present_controller.c'),
  *[str(ROOT/'lib/PortableApps/src'/name) for name in ('quick_actions.c','quick_render.c','quick_session.c')],'-Wl,--wrap=free','-Wl,--wrap=malloc','-o',str(binary)],check=True)
 for case in cases:
  directory=out/f'{case}-{int(sanitized)}'
  if directory.exists():shutil.rmtree(directory)
  directory.mkdir()
  result=subprocess.run([binary,case,source,directory],text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'),timeout=30)
  if result.returncode:raise AssertionError((case,sanitized,result.stdout,result.stderr))
  results.append(dict(sanitized=sanitized,**json.loads(result.stdout)))
  for file in directory.glob('*.pbm'):Image.open(file).rotate(270,expand=True).save(file.with_suffix('.png'))
 baseline=Image.open(out/f'absent-{int(sanitized)}/endpoint.pbm').tobytes()
 for case in ('normal','slow','dropped','padded','malformed','refused','oom','repeat','quick','quick-slow','quick-repeat','quick-back','quick-home','brightness','brightness-slow','alarm'):
  assert Image.open(out/f'{case}-{int(sanitized)}/endpoint.pbm').tobytes()==baseline,case+' failed exact endpoint'
 frames=[Image.open(path).rotate(270,expand=True) for path in sorted((out/f'normal-{int(sanitized)}').glob('frame-*.pbm'))]
 assert len({image.tobytes() for image in frames})>=4
 # Each pixel follows one old-to-new switch, with no invented gray colors.
 old=Image.open(out/f'normal-{int(sanitized)}/outgoing.pbm').tobytes();final=Image.open(out/f'normal-{int(sanitized)}/endpoint.pbm').tobytes()
 previous=old
 for path in sorted((out/f'normal-{int(sanitized)}').glob('frame-*.pbm')):
  current=Image.open(path).tobytes()
  assert all(((x^y)&~(o^f))==0 and ((x^y)&(x^o))==0 for o,f,x,y in zip(old,final,previous,current))
  previous=current
 frames[0].save(out/f'crossfade-{int(sanitized)}.gif',save_all=True,append_images=frames[1:],duration=150,loop=0)
 print(f'Crossfade: {len(cases)} {"ASan/UBSan" if sanitized else "normal"} cases, exact endpoint, intermediate MONO1 pixels PASS',flush=True)
(out/'evidence.json').write_text(json.dumps({'hardware':'not run','cases':results},indent=2)+'\n')
