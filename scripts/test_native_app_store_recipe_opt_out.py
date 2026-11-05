#!/usr/bin/env python3
"""Byte-compare both complete retained updater recipes with native UI deselected."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

import build_native_app_store_qualification as target
from test_native_app_store_opt_out import BASELINE, BASELINE_TREE, materialize_baseline, source_hashes

ROOT=target.ROOT


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()


def build(cc,root,repos,app,out):
    out.mkdir(parents=True)
    include=out/'include';shutil.copytree(root/'lib/PortableApps/include',include)
    shutil.copytree(root/'lib/PortableApps/time',out/'time')
    runtime=out/'runtime-sdk';runtime.mkdir()
    for name in target.git(repos['runtime'],'ls-tree','-r','--name-only',target.PINS['runtime'],'sdk/app').decode().splitlines():
        if name.endswith('.h'):(runtime/Path(name).name).write_bytes(target.pinned(repos,'runtime',name))
    for name in ('RiscRuntimeV1.h','RiscKeyValueAtomicV1.h','RiscKeyValueAvailabilityV1.h'):
        (include/name).write_bytes((runtime/name).read_bytes())
    profile=out/'profile';profile.mkdir()
    for owner,name in [('profile_source','profiles/shared-editor-inputs/'+n) for n in ('watch-alarm-profile.h','AlarmServicePinnedV1.h')]+[
            ('utilities','Apps/alarm_writer.h'),('utilities','lib/Alarm/include/AlarmRecords.h')]:
        (profile/Path(name).name).write_bytes(target.pinned(repos,owner,name))
    (profile/'empty_catalog.c').write_bytes((ROOT/target.CATALOG_PROFILE).read_bytes())
    mapping=out/(app+'.map');mapping.write_text('{ global: app_main; app_module_init; app_module_fini; local: *; };\n')
    original=json.loads((ROOT/target.RECIPE).read_bytes())[app]['command']
    pos=original.index('-o')
    replacements={'@r0@/lib/PortableApps/include':str(include),'@r0@':str(root),
                  '@r6@/sdk/app':str(runtime),'@r2@/Apps':str(profile),'@r2@/lib/Alarm/include':str(profile),
                  '@r2@/lib/Contexts/include':str(runtime),'@r3@':str(repos['bridges']),
                  '@work@/target-updaters/input-fixtures':str(profile),
                  '@work@/target-updaters/fixed/'+app+'.map':str(mapping)}
    args=[]
    for index,arg in enumerate(original[1:],1):
        if index in (pos,pos+1):continue
        for old,new in replacements.items():arg=arg.replace(old,new)
        if '@' in arg:raise RuntimeError('Unresolved retained recipe input')
        args.append(arg)
    args+=['-UPORTABLE_UPDATE_NATIVE_UI','-I'+str(profile),'-I'+str(runtime)]
    staged={str(p.relative_to(out)):sha(p) for p in out.rglob('*') if p.is_file()}
    preprocessing=[]
    for i,arg in enumerate(args):
        if arg.startswith(('-D','-U','-I')) or arg=='-include' or (i and args[i-1]=='-include'):preprocessing.append(arg)
    dependencies=set()
    for source in [x for x in args if x.endswith('.c')]:
        raw=subprocess.check_output([cc,'-std=c11','-MM',*preprocessing,source],text=True)
        dependencies.update(Path(x).resolve() for x in shlex.split(raw.replace('\\\n',' ').split(':',1)[1]))
    bridge_inputs={}
    for p in dependencies:
        if p.is_relative_to(repos['bridges']):
            name=str(p.relative_to(repos['bridges']))
            expected=target.pinned(repos,'bridges',name)
            if p.read_bytes()!=expected:raise RuntimeError('External bridge source differs from pin: '+name)
            bridge_inputs[name]=sha(p)
        elif not p.is_relative_to(root) and not p.is_relative_to(out):
            raise RuntimeError('Unowned full-recipe dependency: '+str(p))
    elf=out/(app+'.elf');command=[cc,*args,'-o',str(elf)]
    result=subprocess.run(command,capture_output=True,text=True,timeout=180)
    (out/'build.stdout').write_text(result.stdout);(out/'build.stderr').write_text(result.stderr)
    (out/'command.json').write_text(json.dumps(command,indent=2)+'\n');result.check_returncode()
    data=elf.read_bytes()
    if data[:7]!=b'\x7fELF\x01\x01\x01' or data[16:20]!=b'\x03\x00\x5e\x00':raise RuntimeError('Wrong target ELF')
    for name,digest in staged.items():
        if sha(out/name)!=digest:raise RuntimeError('Materialized input changed: '+name)
    for name,digest in bridge_inputs.items():
        if sha(repos['bridges']/name)!=digest:raise RuntimeError('Bridge source changed during build')
    return {'sha256':sha(elf),'bytes':len(data),'command':command,'staged_source_sha256':staged,'bridge_source_sha256':bridge_inputs}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in target.PINS:p.add_argument('--'+name.replace('_','-'),type=Path,required=True)
    p.add_argument('--xtensa-cc',required=True);p.add_argument('--baseline-dir',type=Path)
    p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    out=a.output.resolve()
    if out.exists() and any(out.iterdir()):p.error('Use empty output directory')
    out.mkdir(parents=True,exist_ok=True)
    cc=target.compiler_path(a.xtensa_cc);repos={k:getattr(a,k).resolve() for k in target.PINS}
    if target.git(repos['bridges'],'rev-parse','HEAD').decode().strip()!=target.PINS['bridges']:raise RuntimeError('Bridge pin differs')
    if sha(ROOT/target.RECIPE)!=target.RECIPE_SHA256:raise RuntimeError('Retained recipe changed')
    receipt={'schema':1,'status':'RUNNING','baseline_commit':BASELINE,'baseline_tree':BASELINE_TREE,
             'candidate_head_commit':target.git(ROOT,'rev-parse','HEAD').decode().strip(),
             'recipe_sha256':target.RECIPE_SHA256,'native_ui':False,'hardware_verified':False,
             'historical_released_binary_reproduction':False,'compiler':subprocess.check_output([cc,'--version'],text=True).splitlines()[0],
             'compiler_sha256':sha(Path(cc)),'external_pins':target.PINS,'profiles':{}}
    try:
        with tempfile.TemporaryDirectory(prefix='sources-',dir=out) as tmp:
            stage=Path(tmp);candidate=stage/'candidate';baseline=stage/'baseline';baseline.mkdir()
            for directory in ('Apps','lib'):shutil.copytree(ROOT/directory,candidate/directory)
            receipt['materialized_baseline_commit']=materialize_baseline(baseline,a.baseline_dir)
            hashes={'candidate':source_hashes(candidate),'baseline':source_hashes(baseline)}
            if hashes['candidate']!=source_hashes(ROOT):raise RuntimeError('Source changed during snapshot')
            (out/'source-hashes.json').write_text(json.dumps(hashes,indent=2)+'\n')
            receipt['source_hashes_sha256']=sha(out/'source-hashes.json')
            for app in ('app_store','ota_update'):
                row={label:build(cc,root,repos,app,out/app/label) for label,root in (('base',baseline),('candidate',candidate))}
                row['identical_bytes']=(out/app/'base'/(app+'.elf')).read_bytes()==(out/app/'candidate'/(app+'.elf')).read_bytes()
                receipt['profiles'][app]=row
                if not row['identical_bytes']:raise RuntimeError('Full retained '+app+' ELF changed with native UI off')
                print(app+': full retained recipe identical ('+str(row['candidate']['bytes'])+' bytes)',flush=True)
            if hashes['candidate']!=source_hashes(candidate) or hashes['baseline']!=source_hashes(baseline) or hashes['candidate']!=source_hashes(ROOT):raise RuntimeError('Source changed during target comparison')
            receipt['candidate_matches_checkout_at_completion']=True;receipt['status']='PASS'
    except Exception as error:receipt.update(status='FAIL',error=str(error));raise
    finally:(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    for path in out.rglob('*.elf'):path.unlink()


if __name__=='__main__':main()
