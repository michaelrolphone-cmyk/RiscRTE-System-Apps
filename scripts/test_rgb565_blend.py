#!/usr/bin/env python3
"""Exact RGB565 blend arithmetic; normal and ASan/UBSan, no device operations."""
from pathlib import Path
import argparse,os,subprocess,json
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();r=Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=True);rows=[]
for sanitized in [False,True]:
 exe=a.output/('sanitized' if sanitized else 'normal');cmd=[os.environ.get('CC','cc'),'-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(r/'lib/PortableApps/include')]
 if sanitized:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']
 cmd += [str(r/'test/native_apps/rgb565_blend_test.c'),'-o',str(exe)];subprocess.run(cmd,check=True)
 result=subprocess.check_output([str(exe)],text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});rows.append({'sanitized':sanitized,'command':cmd,'result':result.strip()});print(result.strip())
(a.output/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
