#!/usr/bin/env python3
"""Real scene/text hosts: same-focus queued input and true-focus neutralization."""
import argparse,hashlib,json,os,pathlib,shutil,subprocess,sys,tempfile
from scene_sdk import stage_sdk
ROOT=pathlib.Path(__file__).resolve().parents[1]
LEGACY_MODES=('static','external','raster','pending','paused-close','unsubscribe-fault','subscribe','snapshot','foreground','reset')
MODES=('configure-continuity','before-pause','queued','repeat','held','home','back','focus-changed','gap','pause-again','presenting-input','close-paused','overflow','loss-focus-snapshot','loss-focus-foreground','loss-focus-reset','fault-focus-snapshot','loss-close-unsubscribe','loss-close-foreground','loss-close-reset','fault-close-unsubscribe')
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True);results=[]
 with tempfile.TemporaryDirectory(prefix='review-continuity-') as td:
  temp=pathlib.Path(td);inc=stage_sdk(a.runtime,ROOT,temp/'sdk')
  assert (ROOT/'Services/scene_host/fonts/telemetry.inc').is_file(), 'Use the committed delivered font assets'
  for sanitized in ([False,True] if a.sanitize else [False]):
   for profile in ('watch','paper'):
    flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(inc),'-I'+str(ROOT/'sdk/app'),'-I'+str(ROOT/'Services/text_input'),'-I'+str(ROOT/'test/scene')]
    if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
    if profile=='watch':flags+=['-DSCENE_HOST_TELEMETRY=0','-DSCENE_HOST_WATCH_240=1']
    obj=temp/'text.o';subprocess.run([os.environ.get('CC','cc'),*flags,'-Dt5_driver_get=review_text_driver_get','-c',str(ROOT/'Services/text_input/host.c'),'-o',str(obj)],check=True)
    exe=temp/'probe';subprocess.run([os.environ.get('CC','cc'),*flags,'-DSCENE_HOST_SOURCE="'+str(ROOT/'Services/scene_host/host.c')+'"',str(ROOT/'test/text_input/review_checkpoint_handoff_test.c'),str(ROOT/'test/text_input/review_checkpoint_handoff_probe.c'),str(obj),'-o',str(exe)],check=True)
    for mode in MODES:
     r=subprocess.run([str(exe),mode,profile],text=True,capture_output=True,timeout=30);print(r.stdout,end='')
     if r.returncode:print(r.stderr,end='');r.check_returncode()
     results.append({'profile':profile,'mode':mode,'sanitized':sanitized,'result':'PASS'})
    if profile=='paper':
     legacy=temp/'legacy-checkpoint';subprocess.run([os.environ.get('CC','cc'),*flags,str(ROOT/'Services/scene_host/host.c'),str(ROOT/'test/scene/checkpoint_test.c'),'-o',str(legacy)],check=True)
     for mode in LEGACY_MODES:
      r=subprocess.run([str(legacy),mode],text=True,capture_output=True,timeout=30);print(r.stdout,end='')
      if r.returncode:print(r.stderr,end='');r.check_returncode()
      results.append({'profile':profile,'legacy_mode':mode,'sanitized':sanitized,'result':'PASS'})
 evidence={'hardware_tested':False,'scope':'compiled production scene and text hosts; synthetic native transport input','scenarios':results,'source_sha256':{n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in ('Services/scene_host/host.c','Services/text_input/host.c','Services/text_input/checkpoint.inc','sdk/app/RiscSceneLifecycleV1.h','sdk/app/RiscSceneCheckpointV1.h','test/text_input/review_checkpoint_handoff_test.c','test/text_input/review_checkpoint_handoff_probe.c','scripts/test_text_input_handoff_continuity.py','test/scene/checkpoint_test.c','test/scene/lifecycle_test.c')}}
 (a.output/'handoff-continuity-results.json').write_text(json.dumps(evidence,indent=2)+'\n');print(f'{len(results)} handoff continuity cases PASS')
if __name__=='__main__':main()
