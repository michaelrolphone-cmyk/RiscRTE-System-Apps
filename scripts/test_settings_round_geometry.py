#!/usr/bin/env python3
"""Real Settings round geometry and retained-controller proof; no device acceptance."""
from pathlib import Path
import subprocess,os,argparse,json
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];o=a.output.resolve();o.mkdir(parents=True,exist_ok=True);rows=[]
for side,san in [(300,False),(328,False),(330,False),(400,False),(466,False),(480,False),(466,True)]:
 f=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_SETTINGS_NATIVE_GEOMETRY','-DPORTABLE_SETTINGS_ROUND_GEOMETRY','-DPORTABLE_SAFE_VIEWPORT','-DTEST_SETTINGS_DISPLAY_FLAGS=64',f'-DTEST_SETTINGS_RASTER_WIDTH={side}',f'-DTEST_SETTINGS_RASTER_HEIGHT={side}','-DTEST_SETTINGS_INSET=20','-I'+str(r/'lib/PortableApps/include'),'-I'+str(r/'lib/NativeApps/include')]
 if san:f+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']
 exe=o/'layout';subprocess.run(['cc',*f,str(r/'Apps/settings.c'),str(r/'test/native_apps/settings_round_geometry_test.c'),'-o',str(exe)],check=True)
 subprocess.run([exe],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'});exe.unlink();rows.append({'physical_side':side,'sanitized':san,'passed':True})
exe=o/'chords';subprocess.run(['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-I'+str(r/'lib/PortableApps/include'),str(r/'test/native_apps/portable_round_viewport_test.c'),'-o',str(exe)],check=True);subprocess.run([exe],check=True);exe.unlink()
(o/'receipt.json').write_text(json.dumps(rows,indent=2)+'\n')
