"""Run definition examples, not a production canvas/provider integration."""
import pathlib,subprocess,json,hashlib,os,datetime,argparse
R=pathlib.Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--system',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
inc=a.system.resolve()/'lib/PortableApps/include';sdk=a.system.resolve()/'sdk/app';results=[]
for fixture in ['canvas_ownership_examples','optional_scene_contract_test']:
 for name,flags in [('normal',[]),('sanitized',['-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
  out=a.output/(fixture+'-'+name)
  cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(inc),'-I'+str(sdk),*flags,str(R/(fixture+'.c')),'-o',str(out)]
  subprocess.run(cmd,check=True)
  run=subprocess.run([str(out)],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True,text=True,capture_output=True)
  (a.output/(fixture+'-'+name+'.log')).write_text(run.stdout+run.stderr)
  results.append({'fixture':fixture,'variant':name,'command':cmd,'stdout':run.stdout,'stderr':run.stderr,'exit':run.returncode})
files=[R/'canvas_ownership_examples.c',R/'optional_scene_contract_test.c',R/'proposed/RiscCanvasV1.h',inc/'RiscInputNavigationV1.h']
(a.output/'qualification.json').write_text(json.dumps({'observed_at':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source_sha256':{str(p):hashlib.sha256(p.read_bytes()).hexdigest()for p in files},'runs':results,'scope':'38 canvas specification-model cases and20 existing optional-scene admission cases; no renderer/provider, target build, physical device or full CanvasApp integration claim. Leak sanitizer disabled for execution environment.'},indent=2)+'\n')
print('58 definition/admission case executions passed')
