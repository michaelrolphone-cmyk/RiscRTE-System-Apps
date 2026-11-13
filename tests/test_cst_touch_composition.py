"""Actual CST provider plus shared ordered reducer; all hardware media modeled."""
import argparse,subprocess,tempfile,os,json,hashlib,shutil
from pathlib import Path
S=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--drivers',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--consumer',type=Path,default=S);p.add_argument('--expected-regression',action='store_true');a=p.parse_args();D=a.drivers.resolve();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True);sdk=O/'sdk'
 subprocess.run(['python3',str(D/'scripts/cst9217_sdk.py'),'--drivers',str(D),'--runtime',str(a.runtime),'--output',str(sdk)],check=True)
 for n in ['PortableTouch.h','PortableLastAttempt.h','PortableNativeCustody.h']:
  f=a.consumer/'lib/PortableApps/include'/n
  if f.exists():shutil.copyfile(f,sdk/n)
 rows=[]
 for mode in ['normal','sanitized']:
  flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if mode=='sanitized' else []
  if a.expected_regression:flags+=['-DEXPECTED_REGRESSION']
  cmd=['cc','-std=c11','-Wall','-Wextra','-Werror',*flags,'-I'+str(sdk),'-I'+str(D/'components/co5300/include'),'-I'+str(D/'test/drivers'),str(D/'Drivers/cst9217_touch/driver.c'),str(S/'tests/fixtures/cst_touch_composition.c'),str(D/'test/drivers/cst9217_diagnostic_stub.c'),'-o',str(O/mode)]
  subprocess.run(cmd,check=True)
  r=subprocess.run([str(O/mode)],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1'},text=True,capture_output=True);(O/(mode+'.log')).write_text(r.stdout+r.stderr);assert r.returncode==0,r.stdout+r.stderr;rows.append({'mode':mode,'returncode':r.returncode})
 files=[D/'Drivers/cst9217_touch/driver.c',D/'test/drivers/cst9217_test_model.h',S/'tests/fixtures/cst_touch_composition.c',a.consumer/'lib/PortableApps/include/PortableTouch.h',Path(__file__)]
 j={'passed':True,'expected_regression':a.expected_regression,'results':rows,'source_sha256':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},'physical':False};(O/'receipt.json').write_text(json.dumps(j,indent=2)+'\n');print('PASS actual CST/reducer composition normal+sanitized; expected_regression=',a.expected_regression)

if __name__=="__main__":main()
