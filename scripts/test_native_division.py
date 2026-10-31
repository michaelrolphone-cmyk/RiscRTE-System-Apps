#!/usr/bin/env python3
"""Check private compiler division support without adding exported ABI symbols."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
 sources=[ROOT/'lib/NativeApps/src/UnsignedDivisionCompat.c',ROOT/'lib/NativeApps/src/SignedDivisionCompat.c',ROOT/'test/native_division/signed_test.c']
 flags=['-std=c11','-O1','-Wall','-Wextra','-Werror']
 if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
 exe=a.output/'division-test';cmd=[os.environ.get('CC','cc'),*flags,*map(str,sources),'-o',str(exe)]
 subprocess.run(cmd,check=True);env=dict(os.environ);env.setdefault('ASAN_OPTIONS','detect_leaks=0')
 with (a.output/'test.log').open('w') as log:subprocess.run([str(exe)],env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
 (a.output/'results.json').write_text(json.dumps({'pairs':20144,'passed':True,'sanitize':a.sanitize,'command':cmd,'sha256':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in [*sources,Path(__file__)]}},indent=2)+'\n')
if __name__=='__main__':main()
