#!/usr/bin/env python3
"""Exercise real shared update controllers/renderers with fake hardware only."""
import argparse,os,subprocess,json
from pathlib import Path
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
count=0
for native,width,height in ((False,240,240),(True,240,240),(True,410,502),(True,480,600)):
 for kind in (0,1):
  name=f'updates-{native}-{width}-{height}-{kind}';binary=out/name
  flags=['-DTEST_IDLE_ELIGIBILITY','-DPORTABLE_NOVA_UI',f'-DPORTABLE_UPDATE_FIRMWARE={kind}',f'-DUW={width}',f'-DUH={height}']
  if native:flags+=['-DPORTABLE_WATCH_NATIVE_HEIGHT']
  subprocess.run(['/usr/bin/cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=undefined','-fno-sanitize-recover=all',*flags,'-I'+str(r/'lib/PortableApps/include'),'-I'+str(r/'lib/NativeApps/include'),str(r/'test/native_apps/portable_update_app_test.c'),'-o',str(binary)],check=True)
  frames=out/(name+'-frames');frames.mkdir(exist_ok=True)
  for case in [*range(38),40]:
   env={**os.environ,'PORTABLE_UPDATE_FRAME_DIR':str(frames)}
   result=subprocess.run([str(binary),str(case)],env=env,text=True,capture_output=True,timeout=20)
   if result.returncode:print(name,case,result.stdout,result.stderr,flush=True)
   assert result.returncode==0;count+=1
  print(name,'39 scenarios PASS',flush=True)
(out/'results.json').write_text(json.dumps({'scenarios':count,'sanitizer':'undefined','dimensions':[[240,240],[410,502],[480,600]],'hardware':False},indent=2)+'\n')
