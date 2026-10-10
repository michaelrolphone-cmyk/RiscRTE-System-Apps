#!/usr/bin/env python3
"""Qualify selected Home lock, retained minute and GPIO return in real app code.

Strict host providers execute the candidate Clock, shared adapter and product
sleep hook. Target receipts bind every production input. No hardware is used.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]
NAMES = ('adapter.c desk_clock_faces.c PortableRealtimeClient.c PortableTimeZone.c '
         'PortableTimeZoneCatalog.c PortableTimeZonePreference.c quick_actions.c '
         'quick_render.c quick_session.c quick_radios.c').split()

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--x4', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, default=ROOT/'build/home-desk-lock-tests')
    args = parser.parse_args()
    candidate=args.candidate.resolve();x4=args.x4.resolve();runtime=args.runtime.resolve()
    out=args.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
    receipt=json.loads((candidate/'build-evidence.json').read_text())
    include=Path(receipt['paper_transition']['compiled_include_directory'])
    assert '-DPORTABLE_DESK_LOCK_HOME' in receipt['build_defines']
    assert receipt['tagged_alarm_sdk']['api']==2 and receipt['home_points']
    for name,expected in receipt['desk_sources'].items():
        assert sha(ROOT/name)==expected, 'Stale production source: '+name
    for name,expected in receipt['desk_sdk_headers'].items():
        assert sha(include/name)==expected, 'Stale SDK: '+name
    sleep=x4/'minimal/apps/portable_sleep.c'
    assert sha(sleep)==receipt['local_sleep_source_sha256']
    assert sha(candidate/'default.elf')==receipt['sha256']
    flags=[flag for flag in receipt['build_defines'] if flag.startswith('-D')]
    flags+=['-DTEST_NATIVE_LANDSCAPE','-DTEST_HOME_DESK_LOCK']
    sources=[ROOT/'Apps/paper_clock.c',*[ROOT/'lib/PortableApps/src'/name for name in NAMES],
             sleep,ROOT/'test/native_apps/sparse_clock_startup_test.c']
    evidence={'purpose':__doc__,'hardware':'not run','candidate_sha256':receipt['sha256'],
              'production_sources_verified':True,'system_commit':receipt['repository_commit'],
              'sources':{str(path):sha(path) for path in sources},'runs':[]}
    for sanitized in (False,True):
        label='asan-ubsan' if sanitized else 'normal';binary=out/label
        extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
        subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
            *extra,*flags,'-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include'),
            '-I'+str(runtime/'sdk/driver'),'-I'+str(x4/'minimal/drivers/x4pro_power'),
            *map(str,sources),'-o',str(binary)],check=True)
        env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1',CLOCK_EPOCH='1791369720')
        def run(name,case='manual',state=None,**settings):
            directory=out/(label+'-'+name);directory.mkdir(exist_ok=True)
            for pattern in ('*.pixels','*.png'):
                for path in directory.glob(pattern):path.unlink()
            if state is None:
                state=directory/'state.bin';state.unlink(missing_ok=True)
            result=subprocess.run([binary,case,state],env=dict(env,LOCK_CAPTURE=str(directory),RAW_QUICK_FRAME=str(directory/'quick.pixels'),**settings),
                                  capture_output=True,text=True,timeout=20)
            (directory/'run.log').write_text(result.stdout+result.stderr)
            assert result.returncode==0,(label,name,result.stdout,result.stderr)
            if settings.get('LOCK_RETAIN') or settings.get('LOCK_SUBMIT_RETAIN'):
                assert 'Sparse retained manual PASS' in result.stdout
            for path in directory.glob('*.pixels'):
                image=Image.frombytes('1',(800,480),bytes(value^255 for value in path.read_bytes()))
                if path.stem.startswith('home'):image=image.rotate(270,expand=True)
                image.save(path.with_suffix('.png'))
            evidence['runs'].append({'profile':label,'name':name,'case':case,'settings':settings,'result':result.stdout.strip()})
            return directory,state
        homes=[];desks=[]
        for direction in ('0','1'):
            directory,state=run('lock-'+direction,DESK_DIRECTION=direction)
            homes.append(Image.open(directory/'home.png'));desks.append(Image.open(directory/'desk-clean.png'))
            # Use the actual committed retained bytes and image, in a new process.
            for minute in range(1,32):
                run('timer-'+direction+'-'+str(minute),'terminal',state,
                    DESK_DIRECTION='3',CLOCK_ZONE='Poison/Unused')
            run('gpio-'+direction,'gpio',state,DESK_DIRECTION=direction,LOCK_WAKE_HELD='1')
            run('same-minute-'+direction,'terminal',state,LOCK_SAME_MINUTE='1')
            run('queued-'+direction,DESK_DIRECTION=direction,RAW_ASYNC='1')
            run('held-'+direction,DESK_DIRECTION=direction,LOCK_HOLD_MS='2200',LOCK_WAKE_HELD='1',RAW_ASYNC='1')
            for kind,settings in (
                ('refuse',{'LOCK_REFUSE':'1'}),('retained',{'LOCK_RETAIN':'1'}),
                ('repress',{'LOCK_REPRESS':'1'}),('cancel-paint',{'LOCK_CANCEL_PAINT':'1'}),
                ('key-held',{'LOCK_KEY_HELD':'1'}),('submit-retain',{'LOCK_SUBMIT_RETAIN':'1'})):
                returned,_=run(kind+'-'+direction,DESK_DIRECTION=direction,RAW_ASYNC='1',**settings)
                if kind not in ('retained','submit-retain'):
                    before=Image.open(returned/'home.png').crop((0,0,480,474))
                    after=Image.open(returned/'home-restored.png').crop((0,0,480,474))
                    assert ImageChops.difference(before,after).getbbox() is None,kind+' changed restored Home orientation'
        assert ImageChops.difference(homes[0],homes[1]).getbbox() is None,'Desk preference changes Home'
        assert ImageChops.difference(desks[0],desks[1].rotate(180)).getbbox() is None,'Landscape directions are not exact inverses'
        for direction in ('0','1'):
            returned,_=run('portrait-flip-'+direction,HOME_FLIP='1',DESK_DIRECTION=direction,LOCK_REFUSE='1')
            assert ImageChops.difference(Image.open(returned/'home.png').rotate(180),homes[0]).getbbox() is None
            before=Image.open(returned/'home.png').rotate(180).crop((0,0,480,474))
            after=Image.open(returned/'home-restored.png').rotate(180).crop((0,0,480,474))
            assert ImageChops.difference(before,after).getbbox() is None,'Portrait flip lost after refused lock'
        for face in range(1,6):
            pair=[]
            for direction in ('0','1'):
                directory,state=run('face-'+str(face)+'-'+direction,CLOCK_FACE=str(face),DESK_DIRECTION=direction)
                pair.append(Image.open(directory/'desk-clean.png'))
                run('face-minute-'+str(face)+'-'+direction,'terminal',state)
            assert ImageChops.difference(pair[0],pair[1].rotate(180)).getbbox() is None,'Face landscape mismatch'
        for name,settings in (
            ('direction-missing',{'LOCK_DIRECTION_MISSING':'1'}),
            ('direction-corrupt',{'LOCK_DIRECTION_CORRUPT':'1'}),
            ('direction-io',{'LOCK_DIRECTION_IO':'1'}),
            ('direction-range',{'DESK_DIRECTION':'2'})):
            run(name,RAW_ASYNC='1',**settings)
        print(label+': selected Home lock and retained/GPIO lifecycle PASS',flush=True)
    (out/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print(str(len(evidence['runs']))+' candidate-matched host runs PASS: '+str(out/'evidence.json'))

if __name__=='__main__':
    main()
