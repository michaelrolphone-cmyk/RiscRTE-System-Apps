#!/usr/bin/env python3
"""Real native466 Settings timezone controllers, both rotation/sanitizer lanes."""
from pathlib import Path
import subprocess,os,json
import argparse
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];o=a.output.resolve();o.mkdir(parents=True,exist_ok=True);env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'}
flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_SETTINGS_NATIVE_GEOMETRY','-DPORTABLE_SAFE_VIEWPORT','-DTEST_SETTINGS_RASTER_WIDTH=466','-DTEST_SETTINGS_RASTER_HEIGHT=466','-DTEST_SETTINGS_INSET=68','-I'+str(r/'lib/PortableApps/include'),'-I'+str(r/'lib/NativeApps/include')]
# Independent coordinate boundaries and every modeled editor/page, including
# an actual root->Time/Date->Cancel->root Back interaction.
for san in [False,True]:
 exe=o/'layout-test';f=flags+(['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie'] if san else [])
 subprocess.run(['cc',*f,str(r/'Apps/settings.c'),str(r/'test/native_apps/settings_native_geometry_test.c'),'-o',str(exe)],check=True)
 subprocess.run([exe],check=True,env=env);exe.unlink()
rows=[]
for rot,san in [(0,False),(180,True)]:
 exe=o/'timezone-test';f=flags+[f'-DPORTABLE_TOUCH_ROTATION={rot}','-DTEST_TIMEZONE_COMPACT']
 if san:f+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']
 src=[r/'Apps/settings.c',r/'test/native_apps/portable_timezone_settings_test.c']+[r/'lib/PortableApps/src'/n for n in ['PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c']]
 subprocess.run(['cc',*f,*map(str,src),'-o',str(exe)],check=True)
 with (o/f'timezone-{rot}-{san}.log').open('w') as log:
  for mode in [0,1]:
   for index in range(419):subprocess.run([exe,str(mode),str(index)],check=True,stdout=log,stderr=subprocess.STDOUT,env=env)
  for mode in range(2,30):subprocess.run([exe,str(mode)],check=True,stdout=log,stderr=subprocess.STDOUT,env=env)
  if not san:
   frames=o/'timezone-frames';frames.mkdir(exist_ok=True)
   for mode in range(30,37):
    subprocess.run([exe,str(mode)],check=True,stdout=log,stderr=subprocess.STDOUT,env={**env,'TIMEZONE_SETTINGS_FRAMES':str(frames)})
    from PIL import Image
    for p in frames.glob('*.ppm'):
     im=Image.open(p);q=p.with_suffix('.png');im.save(q);assert Image.open(q).tobytes()==im.tobytes();p.unlink()
 rows.append({'rotation':rot,'sanitized':san,'navigation_zones':419,'touch_zones':419,'faults':28});print(rows[-1],flush=True)
(o/'timezone-native-receipt.json').write_text(json.dumps(rows,indent=2)+'\n');exe.unlink()
