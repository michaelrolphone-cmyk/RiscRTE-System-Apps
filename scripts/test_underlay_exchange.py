#!/usr/bin/env python3
"""Owning exact replay underlay exchange identity, fallback and retained-custody proofs."""
from pathlib import Path
import argparse,subprocess,os,json,sys
r=Path(__file__).resolve().parents[1];a=argparse.ArgumentParser();a.add_argument('--output',type=Path,required=True);a.add_argument('--sanitize',action='store_true');a.add_argument('--overwrite-clear',action='store_true');a.add_argument('--nova-mask-bounds',action='store_true');args=a.parse_args();o=args.output.resolve();o.mkdir(parents=True,exist_ok=True)
c=[os.environ.get('CC','cc'),'-std=c11','-O1','-g1','-Wall','-Wextra','-Werror','-Wno-unused-function','-DPORTABLE_RASTER_SNAPSHOT','-DPORTABLE_NOVA_UI','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_ALARM_TERMINAL_RETENTION','-DPORTABLE_RASTER_UNDERLAY_EXCHANGE','-DPORTABLE_FRAME_RECEIPTS','-Wl,--wrap=free','-I'+str(r/'lib/PortableApps/include'),'-I'+str(r/'lib/NativeApps/include')]
if args.nova_mask_bounds:c+=['-DPORTABLE_NOVA_MASK_BOUNDS']
if args.overwrite_clear:c+=['-DPORTABLE_RASTER_OVERWRITE_CLEAR']
if args.sanitize:c+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
c+=[str(r/'test/native_apps/underlay_exchange_test.c'),str(r/'Apps/settings.c')]+[str(r/'lib/PortableApps/src'/n) for n in ['quick_actions.c','quick_render.c','quick_session.c']]+['-o',str(o/'test')];(o/'command.json').write_text(json.dumps(c,indent=2));subprocess.run(c,check=True)
for case in ['repeated','pinned','pinned-partial','quick-alarm','pending','fallback','reject','retained-false','retained-true','completion-failed','zero-token','post-copy-modal','post-copy-pointer']:
 v=subprocess.run([c[-1],case],env=os.environ|{'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'},capture_output=True,text=True,timeout=30);(o/(case+'.log')).write_text(v.stdout+v.stderr);assert v.returncode==0,(case,v.stderr);print(case,'PASS',flush=True)
for mode in ['ordinary','terminal','clipped']:
 c=[sys.executable,str(r/'scripts/test_portable_raster_custody.py'),'--underlay-exchange','--output',str(o/mode)]
 if args.nova_mask_bounds:c+=['--nova-mask-bounds']
 if args.overwrite_clear:c+=['--overwrite-clear']
 if args.sanitize:c+=['--sanitize']
 if mode=='terminal':c+=['--native-terminal']
 if mode=='clipped':c+=['--clipped-begin']
 subprocess.run(c,check=True)
(o/'result.json').write_text(json.dumps(dict(exchange_cases=13,ordinary_cases=18,terminal_cases=10,clipped_cases=1,sanitized=args.sanitize,overwrite_clear=args.overwrite_clear,nova_mask_bounds=args.nova_mask_bounds,passed=True),indent=2))
