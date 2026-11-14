#!/usr/bin/env python3
"""Qualify the actual canvas table and native provider entry with controlled lower capabilities."""
import argparse,datetime,hashlib,json,os,pathlib,subprocess,tempfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
def run(system,output):
 output.mkdir(parents=True,exist_ok=False)
 cases=[]
 with tempfile.TemporaryDirectory(prefix='canvas-provider-tests-') as temporary:
  for name in ('geometry','frames','provider','authority','entry'):
   for sanitized in (False,True):
    binary=pathlib.Path(temporary)/(name+('-san' if sanitized else ''))
    command=[os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror']
    for include in (ROOT/'Services/canvas_host',ROOT/'sdk/app',system/'lib/PortableApps/include',system/'Services/scene_profile'):
     command+=['-I'+str(include)]
    command+=[str(ROOT/'test/canvas_provider'/f'{name}_test.c'),'-o',str(binary)]
    if sanitized:command+=['-fsanitize=address,undefined']
    subprocess.run(command,check=True)
    result=subprocess.run([str(binary)],check=True,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
    cases.append({'test':name,'sanitized':sanitized,'command':command,'stdout':result.stdout,'stderr':result.stderr,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest()})
 sources=list((ROOT/'Services/canvas_host').glob('*'))+list((ROOT/'test/canvas_provider').glob('*'))+[ROOT/'sdk/app/RiscCanvasV1.h',pathlib.Path(__file__)]
 sources+=[system/'lib/PortableApps/include'/name for name in ('PortableSafeViewport.h','RiscDisplayOutputV1.h','RiscTouchV1.h','RiscInputNavigationV1.h','RiscProviderV2.h','RiscRuntimeV1.h')]
 report={'observedAt':datetime.datetime.now(datetime.timezone.utc).isoformat(),'cases':cases,'source_sha256':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},'scope':'Host lower-capability fixtures; no physical device acceptance.'}
 (output/'results.json').write_text(json.dumps(report,indent=2)+'\n')
 print('All10 canvas provider normal/sanitized programs passed')
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--system',type=pathlib.Path,default=ROOT);parser.add_argument('--output',type=pathlib.Path,required=True);args=parser.parse_args();run(args.system.resolve(),args.output.resolve())
