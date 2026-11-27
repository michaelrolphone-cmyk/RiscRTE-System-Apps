#!/usr/bin/env python3
"""Actual bounded generic glyph raster; no hardware or presentation claim."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
runs=[];dependencies=set()
for sanitized in (False,True):
 exe=out/('sanitized' if sanitized else 'normal');dep=exe.with_suffix('.d')
 command=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-MMD','-MF',str(dep),'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'test/native_apps/compact_ascii_glyph_test.c'),'-o',str(exe)]
 if sanitized:command[1:1]=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 subprocess.run(command,check=True)
 result=subprocess.run([str(exe)],text=True,capture_output=True,check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
 (out/(exe.name+'.log')).write_text(result.stdout+result.stderr);print(result.stdout,end='')
 dependencies.update(Path(word) for word in dep.read_text().replace('\\\n',' ').split(':',1)[1].split())
 runs.append({'sanitized':sanitized,'command':command,'result':'passed'})
dependencies.add(Path(__file__))
(out/'evidence.json').write_text(json.dumps({'source_revision':subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip(),'source_dirty':bool(subprocess.check_output(['git','-C',str(ROOT),'status','--porcelain'])),'runs':runs,'formats':['RGB565','MONO1'],'checks':['exact plus draw_text/draw_label geometry','all32 printable punctuation nonempty','legacy alphanumeric/minus/colon/percent full-buffer parity at normal/clipped edges','blank space/control/high bytes','null/zero/negative/128-character limits','stride and allocation guards'],'dependency_sha256':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(dependencies)},'hardware_tested':False},indent=2)+'\n')
