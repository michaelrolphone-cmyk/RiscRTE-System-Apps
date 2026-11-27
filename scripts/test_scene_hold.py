#!/usr/bin/env python3
"""Qualify additive semantic held input using the production scene source."""
import argparse, hashlib, json, os, shlex, subprocess, tempfile
from pathlib import Path
from scene_sdk import stage_sdk
ROOT = Path(__file__).resolve().parents[1]
CASES=('invalid','release','confirm','navigation','motion','gap','timeout','long','update','pause','close','resync','local_fault','native_loss','dependency_loss','queue_fault','reentry','base_reentry','mixed','long_labels','configure','missing_up','sequence_gap','mixed_start','mixed_press','clock_loss','local_fault_native')
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def run(runtime, output, sanitize=False):
    include=stage_sdk(runtime,ROOT,output/'sdk')
    flags=['-std=c11','-O1','-Wall','-Wextra','-Werror']
    if sanitize: flags+=['-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
    env=dict(os.environ);env['ASAN_OPTIONS']='detect_leaks=0'
    commands=[];rows=[];inputs={Path(__file__).resolve(),ROOT/'scripts/scene_sdk.py'}
    def command(args,name):
        commands.append(args)
        result=subprocess.run(args,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        (output/(name+'.log')).write_text(result.stdout)
        if result.returncode: raise RuntimeError(f'{name} failed: see {output/(name+".log")}')
    for profile,pf in [('full',[]),('compact',['-DSCENE_HOST_TELEMETRY=0']),('watch240',['-DSCENE_HOST_TELEMETRY=0','-DSCENE_HOST_WATCH_240=1'])]:
        objects=[]
        for label,src in [('host',ROOT/'test/hold/host_fixture.c'),('test',ROOT/'test/hold/provider_test.c')]:
            obj=output/f'{profile}-{label}.o';dep=obj.with_suffix('.d');objects.append(obj)
            command([os.environ.get('CC','cc'),*flags,*pf,'-I'+str(include),'-MMD','-MF',str(dep),'-c',str(src),'-o',str(obj)],f'{profile}-{label}-compile')
            for token in shlex.split(dep.read_text().replace('\\\n',' ').split(':',1)[1]): inputs.add(Path(token).resolve())
        exe=output/('hold-test-'+profile)
        command([os.environ.get('CC','cc'),*flags,*map(str,objects),'-o',str(exe)],profile+'-link')
        for shape in (range(1) if profile=='watch240' else range(3)):
            for case in CASES:
                name=f'{profile}-{shape}-{case}';command([str(exe),str(shape),case],name)
                rows.append({'profile':profile,'shape':shape,'case':case,'passed':True,'binary_sha256':digest(exe)})
    receipt={'sanitize':sanitize,'leak_sanitizer':False,'cases':rows,'commands':commands,
             'inputs':{str(p):digest(p) for p in sorted(inputs)},'scope':'production scene with deterministic native/provider endpoints; no physical HID claim'}
    (output/'results.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(f'{len(rows)} cases passed')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--sanitize',action='store_true');p.add_argument('--output',type=Path)
    a=p.parse_args()
    if a.output:a.output.mkdir(parents=True,exist_ok=True);run(a.runtime.resolve(),a.output.resolve(),a.sanitize)
    else:
        with tempfile.TemporaryDirectory(prefix='scene-hold-') as t:run(a.runtime.resolve(),Path(t),a.sanitize)
