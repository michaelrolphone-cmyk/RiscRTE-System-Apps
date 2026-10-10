#!/usr/bin/env python3
"""Production Runtime/Graph + real unloadable app, text and scene service ELFs."""
import argparse,json,os,shutil,subprocess
from pathlib import Path
from scene_sdk import stage_sdk
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');p.add_argument('--paper',action='store_true');p.add_argument('--fast-only',action='store_true');a=p.parse_args();runtime=a.runtime.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
inc=stage_sdk(runtime,ROOT,out/'sdk');shutil.copy(ROOT/'sdk/app/RiscTextEntryV1.h',inc)
flags=['-g','-O1','-Wall','-Wextra','-Werror'];san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'] if a.sanitize else []
incs=['-I'+str(inc),'-I'+str(ROOT/'Services/text_input')];cc=os.environ.get('CC','cc')
def build(source,name,extra=()):subprocess.run([cc,'-std=c11',*flags,*san,'-fPIC','-fvisibility=hidden','-shared',*incs,*extra,str(source),'-o',str(out/name)],check=True)
build(ROOT/'Services/scene_host/host.c','scene.elf');build(ROOT/'Services/text_input/host.c','text.elf');build(ROOT/'Services/scene_profile/profile.c','profile.elf',['-DSCENE_PROFILE_ID="profile"']+(['-DSCENE_PROFILE_PAPER=1','-DSCENE_DISPLAY_ROTATION=270'] if a.paper else []));build(ROOT/'test/text_input/runtime_app.c','default.elf');shutil.copy(out/'default.elf',out/'child.elf')
providers=[('display','display.output'),('touch','input.touch.raw'),('nav','input.navigation'),('keyboard','usb.hid.keyboard')]
for name,cap in providers:build(ROOT/'test/text_input/runtime_provider.c',name+'.elf',[f'-DTEST_ID="{name}"',f'-DTEST_CAP="{cap}"'])
def write(name,data):(out/name).write_text(json.dumps(data,indent=2)+'\n')
def req(cap):return {'capability':cap,'api':1}
def manifest(name,cap,requires):return {'type':'driver','id':name,'version':'0.1.0','driver_abi':2,'architecture':'xtensa-esp32s3','file_name':name+'.elf','requires':[req(c) for c in requires],'provides':[req(cap)]}
for name,cap in providers:write(name+'.json',manifest(name,cap,[]))
write('scene.json',manifest('scene-host','ui.scene',[]))
m=json.loads((ROOT/'Services/scene_host/manifest.json').read_text());m['file_name']='scene.elf';write('scene.json',m)
write('text.json',manifest('text-input-host','ui.text-input',['ui.scene','usb.hid.keyboard']));m=json.loads((out/'text.json').read_text());m['file_name']='text.elf';write('text.json',m)
write('profile.json',manifest('profile','ui.presentation-profile',[]))
write('board.json',{'schema':'riscrte.board-hardware','schema_version':1,'board_id':'test','revision':'unspecified','buses':[],'devices':[]})
for name in ['default','child']:write(name+'.json',{'type':'application','id':name,'version':'0.1.0','architecture':'xtensa-esp32s3','file_name':name+'.elf','entry':'app_main','requires':[req('ui.text-input')]+([req('runtime.provider-promotion')] if name=='default' else [])})
write('boot.json',{'board':'board.json','default_app':'default.elf','provider_activation':'eager','drivers':[{'manifest':name+'.json'} for name in ['scene','text','profile',*[x[0] for x in providers]]],'app_capabilities':[{'manifest':name+'.json','grants':[{**req('ui.text-input'),'instance_id':0}]+([{**req('runtime.provider-promotion'),'instance_id':0}] if name=='default' else [])} for name in ['default','child']]})
sources=['bootstrap/Json.cpp','bootstrap/Board.cpp','bootstrap/Runtime.cpp','runtime/streams/AppStreamSessions.cpp','runtime/streams/ProviderQueueHost.cpp','runtime/drivers/ProviderGraphV2.cpp','runtime/drivers/ProviderModuleV2.cpp']
subprocess.run([os.environ.get('CXX','c++'),'-std=c++17',*flags,*san,'-Wno-missing-field-initializers','-O0','-fno-pie','-no-pie','-rdynamic',*incs,'-I'+str(runtime/'src'),'-I'+str(runtime/'lib/ArduinoJson/src'),'-I'+str(runtime/'test/drivers/stubs'),*[str(runtime/'src'/x) for x in sources],str(ROOT/'test/text_input/runtime_test.cpp'),'-ldl','-o',str(out/'test')],check=True)
for activation in ['eager','demand','demand-retained','demand-retained-armed']:
 boot=json.loads((out/'boot.json').read_text());boot['provider_activation']='demand-retained' if activation=='demand-retained-armed' else activation;write('boot.json',boot)
 for mode in (['fast','fast-cancel'] if a.fast_only else ['fast','fast-cancel','plain','hardware','pending','unclosed','retained',
              *['native-'+point for point in ('keyboard_subscribe','keyboard_snapshot','info','subscribe',
                'snapshot','snapshot-fail','foreground','reset','keyboard_poll','keyboard_next',
                'touch_poll','next','navigation_poll','acquire','submit','status','keyboard_unsubscribe','unsubscribe')]]):subprocess.run([str(out/'test'),str(out),mode,activation],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0',**({'TEXT_TEST_PAPER':'1'} if a.paper else {})})
