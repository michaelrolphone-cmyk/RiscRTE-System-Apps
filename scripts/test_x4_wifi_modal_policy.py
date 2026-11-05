#!/usr/bin/env python3
"""Actual Wi-Fi controller/adapter regressions; no network or device I/O."""
import argparse,os,subprocess,json,sys
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--mode',choices=['normal','undefined','asan'],default='normal');a=p.parse_args()
s=Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=False)
flags=[] if a.mode=='normal' else ['-fsanitize='+('undefined' if a.mode=='undefined' else 'address,undefined'),'-fno-omit-frame-pointer']
commands=[]
for atomic in [False,True]:
 binary=a.output/('atomic' if atomic else 'legacy')
 extra=['-DTEST_MODAL_ATOMIC','-I'+str(s/'lib/RemoteFiles/vendor/tinydtls_sha2'),str(s/'lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c')] if atomic else []
 # Clang diagnoses an unused helper in unchanged vendored SHA-256 source.
 warning=['-Wno-unused-function'] if sys.platform=='darwin' and atomic else []
 cmd=[os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',*flags,'-I'+str(s/'lib/PortableApps/include'),'-I'+str(s/'lib/NativeApps/include'),'-I'+str(a.runtime/'sdk/app'),str(s/'test/native_apps/wifi_modal_policy_test.c'),*extra,*warning,'-o',str(binary)]
 commands.append(cmd);subprocess.run(cmd,check=True,timeout=60)
 for n in range(25):
  r=subprocess.run([str(binary),str(n)],capture_output=True,text=True,timeout=20);(a.output/f'{binary.name}-{n}.log').write_text(r.stdout+r.stderr);r.check_returncode()
(a.output/'receipt.json').write_text(json.dumps({'cases':50,'mode':a.mode,'commands':commands,'source':subprocess.check_output(['git','rev-parse','HEAD'],cwd=s,text=True).strip(),'tracked_dirty':bool(subprocess.check_output(['git','diff','HEAD','--name-only'],cwd=s,text=True)),'hardware_tested':False},indent=2)+'\n')
print('Wi-Fi modal policy: 50 cases PASS ('+a.mode+')')
