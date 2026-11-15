#!/usr/bin/env python3
"""Independent tests of the optional text checkpoint suffix and custody fences."""
import argparse,hashlib,json,os,subprocess,tempfile
from pathlib import Path
from scene_sdk import stage_sdk
ROOT=Path(__file__).resolve().parents[1]
MODES=['abi','unsupported','unsupported-continuity','opt-in','flags','usb-flags','usb-activity-race','usb-queue','outcome-race','home-race','key-race','layer','caps','pause-again','close-pausing','close-paused','invalid']
MODES += ['loss-'+site for site in ('enable-configure','enable-usb-snapshot','checkpoint-snapshot','close-subscription','pause-scene','resume-usb-snapshot','resume-scene')]
MODES += ['usb-continuity-'+mode for mode in ('repeated','held','cancel','done','sleep','overflow','close')]
MODES += ['usb-full-'+mode for mode in ('roundtrip','again','close','pause-loss','pause-fault','resume-loss','snapshot-loss','snapshot-fault')]
CLIENT_MODES=['prefix','lifecycle','close-paused']+[f'enable-{x}' for x in ('busy','unavailable','loss','retained','invalid')]+[f'snapshot-{x}' for x in ('loss','retained','busy','size','flags','phase','paused-presenting','paused-busy')]+[f'pause-{x}' for x in ('loss','retained','busy','busy-after-again','invalid')]+[f'resume-{x}' for x in ('loss','retained','busy','invalid')]
MODES += ['fault-'+site for site in ('enable-configure','enable-usb-snapshot','close-subscription','pause-scene','resume-usb-snapshot','resume-scene')]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');p.add_argument('--only',nargs='+');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
 evidence={'scope':'compiled production text host, mocked scene/USB transports','hardware_tested':False,'scenarios':[]}
 with tempfile.TemporaryDirectory(prefix='text-checkpoint-review-') as td:
  temp=Path(td);include=stage_sdk(a.runtime,ROOT,temp/'sdk')
  for sanitize in ([False,True] if a.sanitize else [False]):
   flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(include),'-I'+str(ROOT/'sdk/app'),'-I'+str(ROOT/'Services/text_input')]
   if sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
   exe=temp/'test';subprocess.run([os.environ.get('CC','cc'),*flags,str(ROOT/'Services/text_input/host.c'),str(ROOT/'test/text_input/review_checkpoint_test.c'),'-o',str(exe)],check=True)
   for mode in a.only or MODES:
    result=subprocess.run([str(exe),mode],capture_output=True,text=True,timeout=30);print(result.stdout,end='')
    if result.returncode:print(result.stderr,end='');result.check_returncode()
    evidence['scenarios'].append({'mode':mode,'sanitized':sanitize,'result':'PASS'})
   if not a.only:
    client=temp/'client';subprocess.run([os.environ.get('CC','cc'),*flags,'-I'+str(ROOT/'lib/PortableApps/include'),str(ROOT/'test/text_input/review_checkpoint_client_test.c'),'-o',str(client)],check=True)
    for mode in CLIENT_MODES:
     result=subprocess.run([str(client),mode],capture_output=True,text=True,timeout=30);print(result.stdout,end='')
     if result.returncode:print(result.stderr,end='');result.check_returncode()
     evidence['scenarios'].append({'client_mode':mode,'sanitized':sanitize,'result':'PASS'})
 evidence['source_sha256']={n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in ('Services/text_input/host.c','Services/text_input/checkpoint.inc','sdk/app/RiscTextEntryV1.h','test/text_input/review_checkpoint_test.c','test/text_input/review_checkpoint_client_test.c','lib/PortableApps/include/PortableTextInputCheckpointClient.h')}
 (a.output/'review-checkpoint-results.json').write_text(json.dumps(evidence,indent=2)+'\n');print(f'{len(evidence["scenarios"])} review checkpoint scenarios PASS')
if __name__=='__main__':main()
