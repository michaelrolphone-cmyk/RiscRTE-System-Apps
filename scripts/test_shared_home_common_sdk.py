#!/usr/bin/env python3
"""Compile the actual shared front controller against one selected Runtime SDK."""
import argparse,hashlib,json,shutil,subprocess
from pathlib import Path
from shared_home_sdk import stage_home_headers
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
 a=p.parse_args()
 if a.output.exists():raise ValueError('Fresh common SDK fixture output required')
 a.output.mkdir(parents=True);sdk=a.output/'sdk'
 shutil.copytree(a.runtime/'sdk/app',sdk)
 closure=stage_home_headers(ROOT,sdk)
 if (sdk/'RiscRuntimeV1.h').read_bytes()!=(a.runtime/'sdk/app/RiscRuntimeV1.h').read_bytes():raise ValueError('Selected Runtime header changed')
 rows=[]
 for i,source in enumerate((ROOT/'Apps/shared_home.c',ROOT/'lib/PortableApps/src/PortableHomeBoot.c')):
  command=['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(sdk),'-c',str(source),'-o',str(a.output/(str(i)+'.o'))]
  result=subprocess.run(command,capture_output=True,text=True)
  (a.output/(str(i)+'.log')).write_text(result.stdout+result.stderr)
  if result.returncode:print(result.stdout+result.stderr);result.check_returncode()
  rows.append({'command':command,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest()})
 old=['cc','-std=c11','-Wall','-Wextra','-Werror',
      *['-I'+str(path) for path in (a.runtime/'sdk/app',ROOT/'sdk/app',ROOT/'lib/PortableApps/include')],
      '-c',str(ROOT/'Apps/shared_home.c'),'-o',str(a.output/'old.o')]
 negative=subprocess.run(old,capture_output=True,text=True);text=negative.stdout+negative.stderr
 (a.output/'pre-fix-duplicate-runtime.log').write_text(text)
 if not negative.returncode or 'conflicting types' not in text or 'risc_runtime_health_v1' not in text:raise ValueError('Exact mixed-Runtime-header negative control did not reproduce')
 (a.output/'receipt.json').write_text(json.dumps({'scope':'Actual common C translation units compiled on host; target build remains separate','closure':closure,'commands':rows,'pre_fix_duplicate_runtime_refused':True},indent=2)+'\n')
 print('Actual common controller SDK compile and mixed-header negative control PASS')
if __name__=='__main__':main()
