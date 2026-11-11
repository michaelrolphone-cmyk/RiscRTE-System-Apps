import pathlib,sys,subprocess,os,json,hashlib,datetime,argparse
R=pathlib.Path(__file__).resolve().parent;p=argparse.ArgumentParser();p.add_argument('--system',type=pathlib.Path,required=True);p.add_argument('--runtime',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();S=a.system.resolve();T=a.runtime.resolve();sys.path.insert(0,str(S/'scripts'))
from scene_sdk import stage_sdk
from build_scene_services import HOST_PROFILE_FLAGS
out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);inc=stage_sdk(T,S,out/'sdk');rows=[]
source_files=list((S/'Services/scene_host').rglob('*.c'))+list((S/'Services/scene_host').rglob('*.inc'))+[S/'test/scene/host_test.c',S/'scripts/build_scene_services.py',R/'provider_contract_probe.c',R/'qualify_existing_provider.py']+list(inc.glob('*.h'))
source_identity={str(p):hashlib.sha256(p.read_bytes()).hexdigest()for p in source_files}
for variant in ['normal','sanitized']:
 for profile in ['watch-compact','color-dynamic']:
  exe=out/(profile+'-'+variant);cmd=['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-I'+str(inc),'-DHOST_FIXTURE="'+str(S/'test/scene/host_test.c')+'"',*HOST_PROFILE_FLAGS[profile]]
  if variant=='sanitized':cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
  cmd += [str(S/'Services/scene_host/host.c'),str(R/'provider_contract_probe.c'),'-o',str(exe)];subprocess.run(cmd,check=True)
  dims=[(240,240)]if profile=='watch-compact'else[(240,240),(466,466),(368,448)]
  cases=[(w,h,mode)for w,h in dims for mode in ['absent-navigation','busy-close','gap-recovery']]+[(240,240,'missing-'+str(n))for n in range(4)]
  if profile=='watch-compact':cases.append((466,466,'compact-reject'))
  for w,h,mode in cases:
   run=subprocess.run([str(exe),str(w),str(h),mode],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
   row={'profile':profile,'variant':variant,'width':w,'height':h,'mode':mode,'exit':run.returncode,'stdout':run.stdout,'stderr':run.stderr};rows.append(row)
   (out/'results.json').write_text(json.dumps({'observed_at':datetime.datetime.now(datetime.timezone.utc).isoformat(),'cases':rows,'source_sha256':source_identity,'hardware_tested':False},indent=2)+'\n')
   if run.returncode:print(row);run.check_returncode()
print(len(rows),'actual provider contract cases passed')
