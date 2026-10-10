#!/usr/bin/env python3
"""Exercise the compiled host, copied lifetimes, HID switching and fail-closed paths."""
import argparse,os,subprocess
from pathlib import Path
from scene_sdk import stage_sdk
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--decoupled-attention',action='store_true');p.add_argument('--sanitize',action='store_true');a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True);inc=stage_sdk(a.runtime,ROOT,a.output/'sdk')
flags=['-std=c11','-g','-O1','-Wall','-Wextra','-Werror']
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie']
exe=a.output/'test';subprocess.run([os.environ.get('CC','cc'),*flags,'-I'+str(inc),'-I'+str(ROOT/'sdk/app'),'-I'+str(ROOT/'Services/text_input'),str(ROOT/'Services/text_input/host.c'),str(ROOT/'test/text_input/host_test.c'),'-o',str(exe)],check=True)
for mode in ['invalid','reason-home','reason-back','reason-home-pending','reason-back-pending','reason-legacy-home','plain','pending','scene-fault','close-fault','snapshot-fault','keyboard-fault','held','attach-detach','gap','transient','drain','ascii','shortcuts',*['native-'+site for site in ['subscribe','scene-open','scene-update','scene-next','scene-snapshot','scene-close','unsubscribe','poll','next','snapshot']]]:
 subprocess.run([str(exe),mode],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
# Force revision exhaustion through a test-only inclusion of the implementation.
# No hooks or altered integer widths are compiled into the provider artifact.
overflow=a.output/'overflow-test'
subprocess.run([os.environ.get('CC','cc'),*flags,'-I'+str(inc),'-I'+str(ROOT/'sdk/app'),'-I'+str(ROOT/'Services/text_input'),str(ROOT/'test/text_input/overflow_test.c'),'-o',str(overflow)],check=True)
for mode in ('suspend','action','value'):
 subprocess.run([str(overflow),mode],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

coexist=a.output/"abi-coexistence"
subprocess.run([os.environ.get("CC","cc"),*flags,"-I"+str(ROOT/"sdk/app"),str(ROOT/"test/text_input/abi_coexistence.c"),"-o",str(coexist)],check=True)
subprocess.run([str(coexist)],check=True,env={**os.environ,"ASAN_OPTIONS":"detect_leaks=0"})

client=a.output/'client-test'
subprocess.run([os.environ.get('CC','cc'),*flags,*(['-DEXPECT_DECOUPLED_ATTENTION=1'] if a.decoupled_attention else []),'-I'+str(inc),'-I'+str(ROOT/'lib/PortableApps/include'),str(ROOT/'test/text_input/client_test.c'),'-o',str(client)],check=True)
for mode in ('legacy','short','bad-tag','bad-version','future-version','home','back','home-pending','unsolicited','home-editing','home-accepted','release-fault','attention-pending'):
 subprocess.run([str(client),mode],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
