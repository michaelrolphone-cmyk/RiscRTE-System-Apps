#!/usr/bin/env python3
"""Compile verified background controller and Firmware Update boundary tests."""
import argparse, hashlib, json, os, subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for key in ('runtime','utilities','output'):p.add_argument('--'+key,type=Path,required=True)
p.add_argument('--sanitize',action='store_true');a=p.parse_args()
root=Path(__file__).resolve().parents[1];out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
u=a.utilities.resolve();runtime=a.runtime.resolve()
commands=[]
def run(cmd):
 commands.append([str(x) for x in cmd]);subprocess.run(commands[-1],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
run(['python3',u/'scripts/test_files_background.py','--runtime',runtime,'--system',root,'--utilities',u,'--output',out/'controller',*(['--sanitize'] if a.sanitize else [])])
flags=['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-missing-field-initializers']
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
flags+=['-I'+str(x) for x in (u/'test/files_background',u/'Apps',out/'controller/include',root/'lib/NativeApps/include')]
sources=[root/'test/ota_update_scene/background_test.c']
run(flags+sources+['-lm','-o',out/'test']);run([out/'test'])
(out/'receipt.json').write_text(json.dumps({'commands':commands,'sanitize':a.sanitize,'sources':{str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in [*sources,root/'Apps/ota_update_scene_background.c',root/'Apps/ota_update_scene_background.h']},'scope':'Canonical controller endpoints, Firmware Update unsafe-boundary zero-I/O and required prior pause. No physical hardware or full root qualification.'},indent=2)+'\n')
