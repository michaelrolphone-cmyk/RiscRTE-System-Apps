#!/usr/bin/env python3
import argparse,hashlib,json,os,pathlib,subprocess,tempfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False);cases=[]
with tempfile.TemporaryDirectory() as t:
 for sanitized in (False,True):
  binary=pathlib.Path(t)/('test-san' if sanitized else 'test')
  cmd=['cc','-std=c11','-O2','-Wall','-Wextra','-Werror',*[ '-I'+str(x) for x in (a.runtime/'sdk/app',ROOT/'sdk/app',ROOT/'lib/PortableApps/include')],str(ROOT/'test/home_boot/boot_test.c'),str(ROOT/'lib/PortableApps/src/PortableHomeBoot.c'),'-o',str(binary)]
  if sanitized:cmd+=['-fsanitize=address,undefined']
  subprocess.run(cmd,check=True);r=subprocess.run([str(binary)],check=False,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'));print(r.stdout);print(r.stderr);r.check_returncode();cases.append({'sanitized':sanitized,'command':cmd,'stdout':r.stdout})
(a.output/'results.json').write_text(json.dumps({'cases':cases,'scope':'Native bootstrap fixture; no physical hardware acceptance'},indent=2)+'\n')
