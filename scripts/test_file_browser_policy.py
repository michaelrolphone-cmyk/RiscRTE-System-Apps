#!/usr/bin/env python3
"""Run actual Files/adapter/text-host lifecycle probes with bounded provider fixtures."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--utilities',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--legacy',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
 sources=['Apps/file_browser_portable.inc','Apps/file_browser_shared_input.inc','lib/PortableApps/src/text_input_policy.inc','test/native_apps/file_browser_policy_test.c','test/native_apps/file_browser_policy_scene.inc','scripts/test_file_browser_shared_input.sh']
 hashes={x:hashlib.sha256((ROOT/x).read_bytes()).hexdigest() for x in sources};rows=[]
 common='idle busy-frame owned-handle paused-home retained missing missing-descriptor remote-idle remote-owned remote-expiry'
 for san in (0,1):
  for resident in (0,1):
   name=f'{"resident" if resident else "local"}-{"sanitized" if san else "normal"}';out=a.output/name;env=dict(os.environ,SANITIZE=str(san),FILES_POLICY='1',FILES_RESIDENT=str(resident),FILES_PROFILES='paper-scroll' if resident else 'rgb',FILES_TEST_SOURCE='file_browser_policy_test.c',FILES_SCENARIOS=common+(' context-message remote-low' if not resident else ''),UTILITIES_ROOT=str(a.utilities.resolve()),OUTPUT_DIR=str(out.resolve()),ASAN_OPTIONS='detect_leaks=0')
   log=a.output/(name+'.log')
   with log.open('w') as f:subprocess.run(['bash',str(ROOT/'scripts/test_file_browser_shared_input.sh')],cwd=ROOT,env=env,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=240)
   count=log.read_text().count('PASS ');assert count==(10 if resident else 12);rows.append({'profile':name,'cases':count,'log_sha256':hashlib.sha256(log.read_bytes()).hexdigest()})
  if a.legacy:
   name='legacy-'+('sanitized' if san else 'normal');env=dict(os.environ,SANITIZE=str(san),OUTPUT_DIR=str((a.output/name).resolve()),ASAN_OPTIONS='detect_leaks=0');log=a.output/(name+'.log')
   with log.open('w') as f:subprocess.run(['bash',str(ROOT/'scripts/test_file_browser_shared_input.sh')],cwd=ROOT,env=env,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=240)
   count=log.read_text().count('PASS');assert count==72;rows.append({'profile':name,'cases':count,'log_sha256':hashlib.sha256(log.read_bytes()).hexdigest()})
 assert hashes=={x:hashlib.sha256((ROOT/x).read_bytes()).hexdigest() for x in sources}
 result={'schema':'files.modal-policy.probes.v1','status':'passed','source_sha256':hashes,'runs':rows,'policy_cases':44,'legacy_cases':144 if a.legacy else 0,'actual_production':['Files controller','portable adapter','plaintext host','sharing client','WebDAV/peer backend'],'fixtures':['scene checkpoint API','battery/display/touch/navigation','resident dispatcher','bounded Contexts/broadcast services'],'physical_hardware_tested':False,'sanitizers':'ASan/UBSan; LeakSanitizer disabled for executor ptrace restriction'}
 (a.output/'qualification.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
