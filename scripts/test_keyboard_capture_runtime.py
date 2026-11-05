#!/usr/bin/env python3
"""Compare masked text capture using real Runtime/Graph, scene and exact GT911.

Only physical transports and display are modeled. No target latency claim.
"""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
from scene_sdk import stage_sdk
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for key in ['runtime','gt911','reader','output']:p.add_argument('--'+key,type=Path,required=True)
 p.add_argument('--baseline',type=Path,required=True);p.add_argument('--candidate',type=Path,default=ROOT);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
 runtime=a.runtime.resolve();gt=a.gt911.resolve();out=a.output.resolve();scene=a.candidate.resolve();baseline=a.baseline.resolve();out.mkdir(parents=True,exist_ok=True)
 source_files=[*[p for root in [scene,baseline] for p in sorted((root/'Services/scene_host').glob('*')) if p.is_file()],ROOT/'Services/text_input/host.c',ROOT/'Services/scene_profile/profile.c',gt/'minimal/drivers/x4pro_gt911/driver.c',gt/'minimal/test/gt911_test.c',runtime/'src/bootstrap/Runtime.cpp',runtime/'src/runtime/drivers/ProviderGraphV2.cpp',*[ROOT/'test/scene_timing'/name for name in ['masked_runtime_test.cpp','gt911_backend.c','masked_runtime_app.c','provider.c']],Path(__file__).resolve()]
 source_hashes={str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in source_files}
 (out/'compiled-source-hashes.json').write_text(json.dumps(source_hashes,indent=2)+'\n')
 inc=stage_sdk(runtime,ROOT,out/'sdk');shutil.copyfile(ROOT/'sdk/app/RiscTextEntryV1.h',inc/'RiscTextEntryV1.h')
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
 for label,source in [('baseline',baseline),('candidate',scene)]:
  wrapper=out/(label+'_instrumented.c');wrapper.write_text('#include '+json.dumps(str(source/'Services/scene_host/host.c'))+'\nextern void scene_timing_cpu_pixel(void);\n__attribute__((no_instrument_function)) void __cyg_profile_func_enter(void*f,void*c){(void)c;if(f==(void*)pixel)scene_timing_cpu_pixel();}\n__attribute__((no_instrument_function)) void __cyg_profile_func_exit(void*f,void*c){(void)f;(void)c;}\n')
  build(wrapper,label+'.elf',['-finstrument-functions'])
 build(ROOT/'Services/scene_profile/profile.c','profile.elf',['-DSCENE_PROFILE_ID="profile"','-DSCENE_PROFILE_PAPER=1','-DSCENE_DISPLAY_ROTATION=270'])
 build(ROOT/'test/scene_timing/masked_runtime_app.c','default.elf');build(ROOT/'Services/text_input/host.c','text.elf')
 providers=[('display','display.output'),('touch','input.touch.raw'),('nav','input.navigation')]
 for name,cap in providers:build(ROOT/'test/scene_timing/provider.c',name+'.elf',[f'-DTEST_ID="{name}"',f'-DTEST_CAP="{cap}"'])
 build(gt/'minimal/drivers/x4pro_gt911/driver.c','gt911.o',['-Dt5_driver_get=scene_timing_gt911_get'],False)
 build(ROOT/'test/scene_timing/gt911_backend.c','backend.o',['-Wno-unused-function','-I'+str(gt/'minimal/test')],False)
 def write(n,d):(out/n).write_text(json.dumps(d,indent=2)+'\n')
 def req(c):return {'capability':c,'api':1}
 def manifest(n,c):return {'type':'driver','id':n,'version':'0.1.0','driver_abi':2,'architecture':'xtensa-esp32s3','file_name':n+'.elf','requires':[],'provides':[req(c)]}
 for n,c in providers:write(n+'.json',manifest(n,c))
 m=json.loads((ROOT/'Services/scene_host/manifest.json').read_text());m['file_name']='scene.elf';write('scene.json',m);write('profile.json',manifest('profile','ui.presentation-profile'))
 m=json.loads((ROOT/'Services/text_input/manifest.json').read_text());m['file_name']='text.elf';write('text.json',m)
 write('board.json',{'schema':'riscrte.board-hardware','schema_version':1,'board_id':'test','revision':'unspecified','buses':[],'devices':[]})
 write('default.json',{'type':'application','id':'default','version':'0.1.0','architecture':'xtensa-esp32s3','file_name':'default.elf','entry':'app_main','requires':[req('ui.text-input')]})
 write('boot.json',{'board':'board.json','default_app':'default.elf','provider_activation':'eager','drivers':[{'manifest':n+'.json'} for n in ['text','scene','profile',*[x[0] for x in providers]]],'app_capabilities':[{'manifest':'default.json','grants':[{**req('ui.text-input'),'instance_id':0}]}]})
 sources=['bootstrap/Json.cpp','bootstrap/Board.cpp','bootstrap/Runtime.cpp','runtime/streams/AppStreamSessions.cpp','runtime/streams/ProviderQueueHost.cpp','runtime/drivers/ProviderGraphV2.cpp','runtime/drivers/ProviderModuleV2.cpp']
 command([os.environ.get('CXX','c++'),'-std=c++17',*flags,*san,'-Wno-missing-field-initializers','-O0','-fno-pie','-no-pie','-rdynamic',*incs,'-I'+str(runtime/'src'),'-I'+str(runtime/'lib/ArduinoJson/src'),'-I'+str(runtime/'test/drivers/stubs'),*[str(runtime/'src'/x) for x in sources],str(ROOT/'test/scene_timing/masked_runtime_test.cpp'),str(out/'gt911.o'),str(out/'backend.o'),'-ldl','-o',str(out/'test')])
 results=[]
 baseline_lost=False
 for label in ['baseline','candidate']:
  shutil.copyfile(out/(label+'.elf'),out/'scene.elf')
  for cost in [500,10000]:
   for period,count in [(200,600),(133,180)]:
    for same in [0,1]:
     for dwell in [20,100]:
      # Baseline stress records loss instead of aborting; candidate must preserve
      # all physical tap count and values, including each explicit CLEAR.
      expected=1 if label=='candidate' or cost==500 else 2
      output=command([str(out/'test'),str(out),'2300',str(cost),str(period),str(count),str(same),str(expected),'1',str(dwell)]).strip()
      received=int(output.split('received=')[1].split()[0])
      if label=='baseline' and received<count:baseline_lost=True
      print(label+' '+output,flush=True)
      results.append({'source':label,'pixel_ns':cost,'period_ms':period,'taps':count,'same_key':bool(same),'dwell_ms':dwell,'received':received,'output':output})
 assert baseline_lost,'Expected baseline stress to reproduce a missed capture'
 proof={'cases':results,'sanitized':a.sanitize,'source_sha256':source_hashes,'limits':[
  'Synthetic input only. Every 60th tap is an explicit CLEAR to stay within the real 63-character password capacity; all other taps are Q or alternating Q/W.',
  'GT911 controller model scans at 5ms and latches changed state READY until ACK; actual hardware behavior is not measured.',
  '500ns and 10000ns virtual per-pixel CPU charges and 2ms display transfer slices are explicit test loads, not device timing estimates.',
  'Display completion is modeled at 2300ms. The shipped panel provider and physical transport are not executed.',
  'Real masked text, scene and GT911 providers plus Runtime/Graph execute; GPIO/I2C/sync dependencies are strict transport fixtures.']}
 assert source_hashes=={str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in source_files},'Source changed during qualification'
 (out/'qualification.json').write_text(json.dumps(proof,indent=2)+'\n')
if __name__=='__main__':main()
