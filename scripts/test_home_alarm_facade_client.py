#!/usr/bin/env python3
"""Existing actual API2 client fixture through distinct facade admission."""
import argparse,hashlib,json,os,pathlib,subprocess,tempfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime',type=pathlib.Path,required=True);p.add_argument('--utilities',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=False);results=[]
for terminal in (False,True):
 for sanitized in (False,True):
  name=('terminal' if terminal else 'standard')+('-san' if sanitized else '')
  binary=a.output/name
  cmd=['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-DALARM_SERVICE_TAGGED_V2','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_ALARM_ACQUISITION_CUSTODY','-DTEST_ALARM_FACADE','-DPORTABLE_ALARM_CAPABILITY="alarm.service.facade"',
       '-I'+str(a.runtime/'sdk/app'),'-I'+str(a.utilities/'lib/Alarm/include'),'-I'+str(ROOT/'lib/PortableApps/include'),str(ROOT/'test/native_apps/portable_alarm_v2_test.c'),'-o',str(binary)]
  if terminal:cmd+=['-DPORTABLE_ALARM_TERMINAL_RETENTION']
  if sanitized:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  subprocess.run(cmd,check=True)
  result=subprocess.run([str(binary.resolve())],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
  print(result.stdout);print(result.stderr);result.check_returncode()
  results.append({'name':name,'command':cmd,'stdout':result.stdout,'sha256':hashlib.sha256(binary.read_bytes()).hexdigest()})
(a.output/'receipt.json').write_text(json.dumps({'cases':results,'target_or_hardware_execution':False},indent=2)+'\n')
