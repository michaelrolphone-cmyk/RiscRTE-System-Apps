#!/usr/bin/env python3
"""Selected Home status, automatic-write refusal and clean release boundaries."""
import argparse,os,json,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--target',required=True,type=Path);p.add_argument('--output',required=True,type=Path);p.add_argument('--sanitize',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
r=json.loads((a.target/'build-evidence.json').read_text());inc=a.output/'include';shutil.copytree(r['paper_transition']['compiled_include_directory'],inc,dirs_exist_ok=True)
for h in (ROOT/'lib/PortableApps/include').glob('*.h'):
 if not h.name.startswith(('Risc','AlarmService')) or h.name=='RiscKeyValueAvailabilityV1.h':shutil.copyfile(h,inc/h.name)
flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-unused-function','-I'+str(inc)]
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
exe=a.output/'home-availability';subprocess.run([os.environ.get('CC','cc'),*flags,str(ROOT/'test/native_apps/home_settings_availability_test.c'),'-o',str(exe)],check=True)
for case in 'unavailable pending conflict available recovery bad-status release-failure dirty-release regression'.split():subprocess.run([str(exe),case],check=True,timeout=10,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
