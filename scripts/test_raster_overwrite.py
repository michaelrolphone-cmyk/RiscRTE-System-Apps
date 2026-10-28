#!/usr/bin/env python3
"""Actual adapter: full/partial clear domination and padded failure parity."""
import pathlib,subprocess,json,os,argparse,hashlib
p=argparse.ArgumentParser();p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();r=pathlib.Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=True);records=[]
for selected in (False,True):
 for san in (False,True):
  label=f'selected{int(selected)}-san{int(san)}';exe=a.output/'test';cmd=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-DPORTABLE_TOUCH_SCROLL','-I'+str(r/'lib/PortableApps/include'),'-I'+str(r/'lib/NativeApps/include'),str(r/'test/native_apps/raster_overwrite_test.c'),'-o',str(exe)]
  if selected:cmd+=['-DPORTABLE_RASTER_OVERWRITE_CLEAR']
  if san:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  (a.output/(label+'.command.json')).write_text(json.dumps(cmd,indent=2));v=subprocess.run(cmd,capture_output=True,text=True);(a.output/(label+'.compile.log')).write_text(v.stdout+v.stderr);assert not v.returncode,v.stderr
  v=subprocess.run([str(exe)],capture_output=True,text=True,env=os.environ|{'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'});(a.output/(label+'.log')).write_text(v.stdout+v.stderr);assert not v.returncode,v.stdout+v.stderr
  records.append(dict(selected=selected,sanitized=san,passed=True,output=v.stdout));exe.unlink();print(label,'PASS',flush=True)
(a.output/'receipt.json').write_text(json.dumps(dict(records=records,source_sha256={p:hashlib.sha256((r/p).read_bytes()).hexdigest() for p in ['lib/PortableApps/src/adapter.c','lib/PortableApps/src/raster_snapshot_replay.inc','test/native_apps/raster_overwrite_test.c']}),indent=2))
