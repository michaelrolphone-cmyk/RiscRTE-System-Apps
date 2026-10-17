#!/usr/bin/env python3
"""Compile adapter profiles and compare target imports against an explicit baseline."""
from pathlib import Path
import argparse, subprocess, json
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--baseline-source',type=Path,required=True)
p.add_argument('--xtensa-cc',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();xcc=str(a.xtensa_cc.resolve());nm=xcc.removesuffix('gcc')+'nm'
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);rows=[]
ROOT=Path(__file__).resolve().parents[1]
for name,flags in [('generic',[]),('native-height',['-DPORTABLE_NOVA_UI','-DPORTABLE_WATCH_NATIVE_HEIGHT']),('native-raster',['-DPORTABLE_NOVA_UI','-DPORTABLE_RF_NATIVE_CANVAS','-DPORTABLE_RASTER_SNAPSHOT'])]:
 imports={}
 for label,root in [('before',a.baseline_source.resolve()),('after',ROOT)]:
  obj=out/(name+'-'+label+'.o');cmd=[xcc,'-std=c11','-Os','-ffunction-sections','-fdata-sections','-mlongcalls','-fPIC',*flags,'-I'+str(root/'lib/PortableApps/include'),'-I'+str(root/'lib/NativeApps/include'),'-c',str(root/'lib/PortableApps/src/adapter.c'),'-o',str(obj)]
  subprocess.run(cmd,check=True);imports[label]=subprocess.check_output([nm,'-u',str(obj)],text=True).splitlines()
 assert imports['before']==imports['after'],(name,imports)
 rows.append({'profile':name,'imports_identical':True,'imports':imports['after']});print(name,'GCC8.4 target compile and identical imports PASS',flush=True)
(out/'results.json').write_text(json.dumps({'compiler':subprocess.check_output([xcc,'--version'],text=True).splitlines()[0],'results':rows},indent=2)+'\n')
