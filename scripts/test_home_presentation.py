#!/usr/bin/env python3
import argparse,json,hashlib,os,pathlib,subprocess,tempfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False);cases=[]
with tempfile.TemporaryDirectory() as temp:
 for view,namespace in [(1,61),(2,62),(1,99),(1,0),(2,0)]:
  for sanitized in (False,True):
   binary=pathlib.Path(temp)/(str(view)+'-'+str(namespace)+'-'+str(sanitized))
   command=['cc','-std=c11','-Wall','-Wextra','-Werror','-O1','-I'+str(ROOT/'sdk/app'),'-I'+str(ROOT/'lib/PortableApps/include'),
    f'-DHOME_POLICY_VIEW={view}',f'-DHOME_POLICY_CRASH_NAMESPACE={namespace}',f'-DHOME_POLICY_POINTS_SCHEMA={1 if view==1 else 3}','-DHOME_POLICY_ID="test-policy"',
    str(ROOT/'Services/home_presentation/policy.c'),str(ROOT/'test/home_presentation/selection_test.c'),'-o',str(binary)]
   if sanitized:command+=['-fsanitize=address,undefined','-fno-sanitize-recover=all']
   subprocess.run(command,check=True);result=subprocess.run([str(binary)],check=True,text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
   print(result.stdout);cases.append({'default_view':view,'namespace':namespace,'sanitized':sanitized,'command':command,'stdout':result.stdout,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest()})
sources=[ROOT/'sdk/app/RiscHomePresentationV1.h',ROOT/'lib/PortableApps/include/PortableHomeSelection.h',ROOT/'Services/home_presentation/policy.c',ROOT/'test/home_presentation/selection_test.c',pathlib.Path(__file__)]
(a.output/'results.json').write_text(json.dumps({'cases':cases,'source_sha256':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in sources}},indent=2)+'\n')
