#!/usr/bin/env python3
"""Qualify paper profile/optional-input/cadence intersection without changing providers."""
import argparse,datetime,hashlib,json,os,pathlib,subprocess,sys,tempfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
def run(system,runtime,output):
 sys.path.insert(0,str(system/'scripts'))
 from scene_sdk import stage_sdk
 from build_scene_services import PROFILE_FLAGS,HOST_PROFILE_FLAGS
 output.mkdir(parents=True,exist_ok=False);rows=[]
 with tempfile.TemporaryDirectory(prefix='paper-provider-contract-') as temporary:
  temporary=pathlib.Path(temporary);include=stage_sdk(runtime,system,temporary/'sdk')
  for sanitized in (False,True):
   flags=['-std=c11','-O1','-Wall','-Wextra','-Werror','-I'+str(include)]
   if sanitized:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
   profile=temporary/('profile-san.o' if sanitized else 'profile.o');binary=temporary/('probe-san' if sanitized else 'probe')
   subprocess.run(['cc',*flags,*PROFILE_FLAGS['portrait-monochrome'],'-Dt5_driver_get=paper_profile_get','-c',str(system/'Services/scene_profile/profile.c'),'-o',str(profile)],check=True)
   subprocess.run(['cc',*flags,*HOST_PROFILE_FLAGS['full'],'-DHOST_FIXTURE="'+str(system/'test/scene/host_test.c')+'"',str(system/'Services/scene_host/host.c'),str(ROOT/'test/scene/paper_contract_probe.c'),str(profile),'-o',str(binary)],check=True)
   for mode in ('touch','static','overlay','pending','missing-0','missing-1','missing-2','missing-3'):
    result=subprocess.run([str(binary),mode],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
    rows.append({'sanitized':sanitized,'mode':mode,'exit':result.returncode,'stdout':result.stdout,'stderr':result.stderr,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'profile_object_sha256':hashlib.sha256(profile.read_bytes()).hexdigest()})
    (output/'results.json').write_text(json.dumps({'observedAt':datetime.datetime.now(datetime.timezone.utc).isoformat(),'cases':rows,'profile_flags':PROFILE_FLAGS['portrait-monochrome'],'host_flags':HOST_PROFILE_FLAGS['full'],'physical_testing':False},indent=2)+'\n')
    result.check_returncode()
  files=list((system/'Services/scene_host').rglob('*.c'))+list((system/'Services/scene_host').rglob('*.inc'))+[system/'Services/scene_profile/profile.c',system/'test/scene/host_test.c',system/'scripts/build_scene_services.py',system/'scripts/scene_sdk.py',ROOT/'test/scene/paper_contract_probe.c',pathlib.Path(__file__)]+list(include.glob('*.h'))
  (output/'source-identity.json').write_text(json.dumps({str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in files},indent=2)+'\n')
 print(len(rows),'paper profile contract cases passed')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--system',type=pathlib.Path,default=ROOT);p.add_argument('--runtime',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();run(a.system.resolve(),a.runtime.resolve(),a.output.resolve())
