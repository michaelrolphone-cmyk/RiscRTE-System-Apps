#!/usr/bin/env python3
"""Real Runtime/Graph, scene, exact GT911; deterministic CPU and register cadence.

Only physical transports and display are modeled. No target latency claim.
"""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
from scene_sdk import stage_sdk
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for key in ['runtime','gt911','reader','output']:p.add_argument('--'+key,type=Path,required=True)
 p.add_argument('--rgb565',action='store_true');p.add_argument('--scene',type=Path,default=ROOT);p.add_argument('--sanitize',action='store_true');p.add_argument('--baseline',action='store_true');a=p.parse_args()
 runtime=a.runtime.resolve();gt=a.gt911.resolve();out=a.output.resolve();scene=a.scene.resolve();out.mkdir(parents=True,exist_ok=True)
 inc=stage_sdk(runtime,ROOT,out/'sdk')
 for folder,names in [(a.reader/'sdk/driver',['RiscI2cBusV1.h','RiscTouchPowerV1.h'])]:
  for src in [folder/name for name in names]:
   dst=inc/src.name
   if dst.exists():assert dst.read_bytes()==src.read_bytes(),src
   else:shutil.copyfile(src,dst)
 flags=['-g','-O1','-Wall','-Wextra','-Werror'];san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'] if a.sanitize else []
 incs=['-I'+str(inc)];cc=os.environ.get('CC','cc');env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'};commands=[]
 def command(args):
  commands.append(list(map(str,args)));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  r=subprocess.run(args,capture_output=True,text=True,env=env)
  if r.returncode:
   (out/'failure.json').write_text(json.dumps({'args':commands[-1],'code':r.returncode,'stdout':r.stdout,'stderr':r.stderr},indent=2)+'\n');raise RuntimeError(r.stderr)
  return r.stdout
 def build(src,name,extra=(),shared=True):command([cc,'-std=c11',*flags,*san,*incs,*extra,*(['-fPIC','-fvisibility=hidden','-shared'] if shared else ['-c']),str(src),'-o',str(out/name)])
 wrapper=out/'scene_instrumented.c';wrapper.write_text('#include '+json.dumps(str(scene/'Services/scene_host/host.c'))+'\nextern void scene_timing_cpu_pixel(void);\n__attribute__((no_instrument_function)) void __cyg_profile_func_enter(void*f,void*c){(void)c;if(f==(void*)pixel)scene_timing_cpu_pixel();}\n__attribute__((no_instrument_function)) void __cyg_profile_func_exit(void*f,void*c){(void)f;(void)c;}\n')
 build(wrapper,'scene.elf',['-finstrument-functions']);build(ROOT/'Services/scene_profile/profile.c','profile.elf',['-DSCENE_PROFILE_ID="profile"','-DSCENE_PROFILE_PAPER=1','-DSCENE_DISPLAY_ROTATION=270']);build(ROOT/'test/scene_timing/runtime_app.c','default.elf')
 providers=[('display','display.output'),('touch','input.touch.raw'),('nav','input.navigation')]
 for name,cap in providers:build(ROOT/'test/scene_timing/provider.c',name+'.elf',[f'-DTEST_ID="{name}"',f'-DTEST_CAP="{cap}"'])
 build(gt/'minimal/drivers/x4pro_gt911/driver.c','gt911.o',['-Dt5_driver_get=scene_timing_gt911_get'],False)
 build(ROOT/'test/scene_timing/gt911_backend.c','backend.o',['-Wno-unused-function','-I'+str(gt/'minimal/test')],False)
 def write(n,d):(out/n).write_text(json.dumps(d,indent=2)+'\n')
 def req(c):return {'capability':c,'api':1}
 def manifest(n,c):return {'type':'driver','id':n,'version':'0.1.0','driver_abi':2,'architecture':'xtensa-esp32s3','file_name':n+'.elf','requires':[],'provides':[req(c)]}
 for n,c in providers:write(n+'.json',manifest(n,c))
 m=json.loads((ROOT/'Services/scene_host/manifest.json').read_text());m['file_name']='scene.elf';write('scene.json',m);write('profile.json',manifest('profile','ui.presentation-profile'))
 write('board.json',{'schema':'riscrte.board-hardware','schema_version':1,'board_id':'test','revision':'unspecified','buses':[],'devices':[]})
 write('default.json',{'type':'application','id':'default','version':'0.1.0','architecture':'xtensa-esp32s3','file_name':'default.elf','entry':'app_main','requires':[req('ui.scene')]})
 write('boot.json',{'board':'board.json','default_app':'default.elf','provider_activation':'eager','drivers':[{'manifest':n+'.json'} for n in ['scene','profile',*[x[0] for x in providers]]],'app_capabilities':[{'manifest':'default.json','grants':[{**req('ui.scene'),'instance_id':0}]}]})
 sources=['bootstrap/Json.cpp','bootstrap/Board.cpp','bootstrap/Runtime.cpp','runtime/streams/AppStreamSessions.cpp','runtime/streams/ProviderQueueHost.cpp','runtime/drivers/ProviderGraphV2.cpp','runtime/drivers/ProviderModuleV2.cpp']
 command([os.environ.get('CXX','c++'),'-std=c++17',*flags,*san,'-Wno-missing-field-initializers','-O0','-fno-pie','-no-pie','-rdynamic',*incs,'-I'+str(runtime/'src'),'-I'+str(runtime/'lib/ArduinoJson/src'),'-I'+str(runtime/'test/drivers/stubs'),*[str(runtime/'src'/x) for x in sources],str(ROOT/'test/scene_timing/runtime_test.cpp'),str(out/'gt911.o'),str(out/'backend.o'),'-ldl','-o',str(out/'test')])
 results=[]
 cases=[(200,600),(133,180),(80,180),(60,180)]
 for cost in ([500] if a.baseline else [0,500]):
  for delay in [1,17,2300]:
   for period,count in cases:
    for same in [0,1]:
     expected=not a.baseline
     output=command([str(out/'test'),str(out),str(delay),str(cost),str(period),str(count),str(same),str(int(expected)),str(5 if a.rgb565 else 1)]).strip();print(output,flush=True);results.append(output)
 proof={'cases':results,'sanitized':a.sanitize,'baseline':a.baseline,'source_sha256':{str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in [scene/'Services/scene_host/host.c',gt/'minimal/drivers/x4pro_gt911/driver.c',ROOT/'test/scene_timing/runtime_test.cpp',ROOT/'test/scene_timing/gt911_backend.c']},'limits':['Physical GT911 reports are simulated: changed state latches READY until ACK; no physical device used.','Per-pixel virtual CPU cost and display transfer slices are explicit test loads, not measurements.','Real Runtime/Graph controls app/scene/profile lifetimes; thin touch fixture owns exact GT911 start/quiesce and its strict scoped GPIO/I2C/sync dependencies.']}
 (out/'qualification.json').write_text(json.dumps(proof,indent=2)+'\n')
if __name__=='__main__':main()
