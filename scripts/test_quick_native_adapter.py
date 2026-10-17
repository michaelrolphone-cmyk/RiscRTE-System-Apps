#!/usr/bin/env python3
"""Production generic-adapter native Quick selection, pixels and raw touch."""
import argparse, hashlib, json, os, shlex, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--normal-only',action='store_true');a=p.parse_args()
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
s=(ROOT/'test/native_apps/portable_handoff_test.c').read_text()
replacements={
 '#include "../../lib/PortableApps/src/adapter.c"':'#include '+json.dumps(str(ROOT/'lib/PortableApps/src/adapter.c')),
 'mock_pixels[240*243]':'*mock_pixels','sizeof(mock_pixels)':'geometry_size',
 '.width=240,.height=240':'.width=geometry_width,.height=geometry_height',
 '.stride_bytes=486,':'.stride_bytes=geometry_stride,',
 'mock_owned[4]':'mock_owned[128]','i<4':'i<128',
 '.acquire=mock_acquire,.release=mock_release':'.acquire=nqa_acquire,.release=mock_release',
 '.present_status=mock_status};':'.present_status=mock_status,.set_brightness=nqa_brightness};'}
for before,after in replacements.items():
 assert before in s,before;s=s.replace(before,after)
fixture=out/'fixture.c';fixture.write_text(s)
rows=[];dependencies={}
for profile,selection in [('native',['-DPORTABLE_NATIVE_CANVAS','-DPORTABLE_QUICK_NATIVE']),('height',['-DPORTABLE_WATCH_NATIVE_HEIGHT','-DPORTABLE_QUICK_NATIVE']),('off',['-DPORTABLE_WATCH_NATIVE_HEIGHT'])]:
 for snapshot in (False,True):
  for sanitized in ((False,) if a.normal_only else (False,True)):
   name=f'{profile}-snapshot{int(snapshot)}-san{int(sanitized)}';binary=out/'test-binary'
   flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
   cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function',*flags,*selection,
    '-DPORTABLE_NOVA_UI','-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_RETAINED_RGB565_HANDOFF',
    *(['-DPORTABLE_RASTER_SNAPSHOT'] if snapshot else []),'-DFRAME_FIXTURE='+json.dumps(str(fixture)),
    '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),
    str(ROOT/'test/native_apps/quick_native_adapter_test.c'),*[str(ROOT/'lib/PortableApps/src'/n) for n in ('quick_actions.c','quick_render.c','quick_session.c')],'-o',str(binary)]
   dep=subprocess.run([cmd[0],'-MM',*cmd[1:-2]],text=True,capture_output=True,check=True)
   (out/(name+'.d')).write_text(dep.stdout)
   selected=[]
   for token in shlex.split(dep.stdout.replace('\\\n',' ')):
    if token.endswith(':'):continue
    path=Path(token).resolve()
    key='generated:fixture.c' if path==fixture else str(path.relative_to(ROOT))
    dependencies[key]=hashlib.sha256(path.read_bytes()).hexdigest();selected.append(key)
   built=subprocess.run(cmd,text=True,capture_output=True);(out/(name+'-build.log')).write_text(built.stdout+built.stderr);built.check_returncode()
   result=subprocess.run([str(binary)],text=True,capture_output=True,timeout=180,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
   (out/(name+'.log')).write_text(result.stdout+result.stderr);print(name,result.stdout+result.stderr,flush=True);result.check_returncode()
   rows.append({'profile':profile,'snapshot':snapshot,'sanitized':sanitized,'passed':True,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'compiler_dependencies':sorted(set(selected))});binary.unlink()
guards=[]
for name,defines,message in (
 ('without-actions',['PORTABLE_QUICK_NATIVE'],'Native Quick requires explicitly selected Quick Actions'),
 ('sparse-start',['PORTABLE_QUICK_NATIVE','PORTABLE_QUICK_ACTIONS','PORTABLE_DESK_CLOCK_SPARSE_START'],'Native Quick requires generic foreground initialization')):
 cmd=[os.environ.get('CC','cc'),'-E',*('-D'+d for d in defines),'-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'lib/PortableApps/src/adapter.c')]
 result=subprocess.run(cmd,text=True,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
 (out/('guard-'+name+'.log')).write_text(result.stderr)
 assert result.returncode and message in result.stderr,name
 guards.append({'profile':name,'rejected':True,'diagnostic':message})
paths=['lib/PortableApps/src/'+n for n in ('adapter.c','foreground_adapter_open.inc','quick_adapter.inc','quick_render.c','quick_native_render.inc','quick_actions.c','quick_session.c','raster_snapshot_replay.inc')]
paths+=['lib/PortableApps/include/'+n for n in ('PortableQuickRender.h','PortableQuickActions.h','PortableQuickLayout.h','PortableQuickSession.h')]
paths+=['test/native_apps/quick_native_adapter_test.c','test/native_apps/portable_handoff_test.c','scripts/test_quick_native_adapter.py']
(out/'results.json').write_text(json.dumps({'rows':rows,'guards':guards,'hardware':False,'source_sha256':{n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in paths},'compiler_dependency_sha256':dependencies,'dependency_scope':'All compiler -MM non-system dependencies for every built configuration; generated fixture is derived from the recorded source fixture and runner.'},indent=2)+'\n')
