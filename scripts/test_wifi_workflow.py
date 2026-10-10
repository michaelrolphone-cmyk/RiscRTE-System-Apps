#!/usr/bin/env python3
"""Run copied-provider Wi-Fi workflows and growing-profile fault tests; no network I/O."""
import argparse,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
root=Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=False)
flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie'] if a.sanitize else []
commands=[]
for name,cases in [('portable_wifi_profiles_test',[None]),('wifi_workflow_test',range(16))]:
 binary=a.output/name
 cmd=[os.environ.get('CC','cc'),'-std=c11','-DTEST_IDLE_ELIGIBILITY','-Wall','-Wextra','-Werror',*flags,'-I'+str(root/'lib/PortableApps/include'),'-I'+str(root/'lib/NativeApps/include'),str(root/'test/native_apps'/f'{name}.c'),'-o',str(binary)]
 commands.append(cmd);subprocess.run(cmd,check=True,timeout=60)
 for case in cases:
  cmd=[str(binary),*([str(case)] if case is not None else [])];commands.append(cmd)
  run=subprocess.run(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=30)
  (a.output/f'{name}-{case}.log').write_text(run.stdout);print(run.stdout,end='');run.check_returncode()
(a.output/'receipt.json').write_text(json.dumps({'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=root,text=True)),'sanitized':a.sanitize,'workflow_cases':16,'profile_collection':50,'commands':commands},indent=2)+'\n')
