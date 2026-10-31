#!/usr/bin/env python3
"""Actual Runtime/app/inventory/scene; update, WiFi and physical endpoints doubled."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
from app_store_scene_support import stage
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');p.add_argument('--utilities',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);rt=a.runtime.resolve();inc=stage(rt,ROOT,out/'sdk',a.utilities);fix=ROOT/'test/app_store_runtime'
tracked=set([Path(__file__).resolve(),*fix.glob('*'),*ROOT.glob('Apps/app_store*')]);tracked.update(p for base in [ROOT/'Services/scene_host',ROOT/'Services/scene_profile',rt/'src',rt/'sdk'] for p in base.rglob('*') if p.is_file());initial={str(p.resolve()):hashlib.sha256(p.read_bytes()).hexdigest() for p in tracked if p.is_file()};(out/'initial-source.json').write_text(json.dumps(initial,indent=2)+'\n')
flags=['-O0','-g0','-pipe','-Wall','-Wextra','-Werror'];san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'] if a.sanitize else [];incs=['-I'+str(inc),'-I'+str(ROOT/'Services/scene_profile'),'-I'+str(ROOT/'Services/text_input'),'-I'+str(ROOT/'lib/Wifi/include')];commands=[];env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'}
def run(cmd):
 commands.append(list(map(str,cmd)));(out/'commands.json').write_text(json.dumps(commands,indent=2));r=subprocess.run(cmd,text=True,capture_output=True,env=env);(out/f'command-{len(commands):03}.log').write_text(r.stdout+r.stderr);r.check_returncode();return r.stdout
obj=out/'objects';obj.mkdir(exist_ok=True)
actual_dependencies={}
def dependencies(compiler,src,options):
 dep=obj/('closure-'+str(len(actual_dependencies))+'.d')
 run([compiler,*options,'-MM','-MF',str(dep),str(src)])
 for path in dep.read_text().replace('\\\n',' ').split(':',1)[1].split():
  p=Path(path).resolve();actual_dependencies[str(p)]=hashlib.sha256(p.read_bytes()).hexdigest()
 (out/'actual-dependencies.json').write_text(json.dumps(actual_dependencies,indent=2)+'\n')
def build(src,name,extra=[]):
 for unit in src:dependencies('cc',unit,['-std=c11',*incs,*extra])
 run(['cc','-std=c11',*flags,*san,*incs,*extra,'-fPIC','-fvisibility=hidden','-shared','-MMD','-MF',str(obj/(name+'.d')),*map(str,src),'-o',str(obj/name)])
build([fix/'app.c',ROOT/'Apps/app_store_inventory.c'],'default.elf')
build([ROOT/'Services/scene_host/host.c'],'scene.elf')
for paper in [0,1]:build([ROOT/'Services/scene_profile/profile.c'],f'profile{paper}.elf',['-DSCENE_PROFILE_ID="profile"',f'-DSCENE_PROFILE_PAPER={paper}','-DSCENE_DISPLAY_ROTATION=0'])
providers=[('display','display.output'),('touch','input.touch.raw'),('nav','input.navigation'),('wifi','net.wifi'),('update','software.update.apps')]
for n,c in providers:build([fix/'provider.c'],n+'.elf',[f'-DTEST_ID="{n}"',f'-DTEST_CAP="{c}"'])
sources=['bootstrap/Json.cpp','bootstrap/Board.cpp','bootstrap/Runtime.cpp','runtime/streams/AppStreamSessions.cpp','runtime/streams/ProviderQueueHost.cpp','runtime/drivers/ProviderGraphV2.cpp','runtime/drivers/ProviderModuleV2.cpp']
cpp_includes=[*incs,'-I'+str(rt/'src'),'-I'+str(rt/'lib/ArduinoJson/src'),'-I'+str(rt/'test/drivers/stubs'),'-I'+str(rt/'test'),'-I'+str(fix)]
for unit in [*[rt/'src'/x for x in sources],fix/'runtime_test.cpp']:dependencies('c++',unit,['-std=c++17',*cpp_includes])
run(['c++','-std=c++17',*flags,*san,'-Wno-missing-field-initializers','-fno-pie','-no-pie','-rdynamic',*incs,'-I'+str(rt/'src'),'-I'+str(rt/'lib/ArduinoJson/src'),'-I'+str(rt/'test/drivers/stubs'),'-I'+str(rt/'test'),'-I'+str(fix),*[str(rt/'src'/s) for s in sources],str(fix/'runtime_test.cpp'),'-ldl','-o',str(out/'test')])
def req(c):return {'capability':c,'api':1}
def manifest(n,c):return {'type':'driver','id':n,'version':'0.1.0','driver_abi':2,'architecture':'xtensa-esp32s3','file_name':n+'.elf','requires':[],'provides':[req(c)]}
def write(d,n,v):(d/n).write_text(json.dumps(v,indent=2)+'\n')
results=[]
for label,w,h,f in [('paper',800,480,1),('watch',240,240,5),('wave',410,502,5)]:
 for activation in ['demand','demand-retained']:
  for mode in ['cancel','begin-cancel','no-ui']:
   d=out/f'{label}-{activation}-{mode}';d.mkdir(exist_ok=True)
   for n in ['default','scene',*[n for n,c in providers]]:
    dest=d/(n+'.elf')
    if dest.exists():dest.unlink()
    os.link(obj/(n+'.elf'),dest)
   shutil.copyfile(obj/f'profile{int(f==1)}.elf',d/'profile.elf')
   for n,c in providers:write(d,n+'.json',manifest(n,c))
   m=json.loads((ROOT/'Services/scene_host/manifest.json').read_text());m['file_name']='scene.elf';write(d,'scene.json',m);write(d,'profile.json',manifest('profile','ui.presentation-profile'))
   write(d,'board.json',{'schema':'riscrte.board-hardware','schema_version':1,'board_id':'test','revision':'unspecified','buses':[],'devices':[]})
   caps=['ui.scene','software.update.apps','storage.key-value','runtime.realtime','net.wifi','software.installed-apps']
   write(d,'default.json',{'type':'application','id':'default','version':'0.1.0','architecture':'xtensa-esp32s3','file_name':'default.elf','entry':'app_main','requires':[],'optional':[req(c) for c in caps]})
   grants=[{**req(c),'instance_id':i} for c,i in [('software.update.apps',0),('storage.key-value',1),('storage.key-value',6),('runtime.realtime',0),('net.wifi',0)]]
   if mode!='no-ui':grants.append({**req('ui.scene'),'instance_id':0})
   write(d,'boot.json',{'board':'board.json','default_app':'default.elf','provider_activation':activation,'drivers':[{'manifest':n+'.json'} for n in ['scene','profile',*[n for n,c in providers]]],'app_capabilities':[{'manifest':'default.json','grants':grants}]})
   result=run([str(out/'test'),str(d),mode,str(w),str(h),str(f)]).strip();print(result,flush=True);results.append(result)
paths=set([Path(__file__).resolve(),*fix.glob('*'),*[rt/'src'/s for s in sources]])
for dep in obj.glob('*.d'):
 for name in dep.read_text().replace('\\\n',' ').split(':',1)[1].split():paths.add(Path(name).resolve())
for path,digest in actual_dependencies.items():assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest,'Compiled dependency changed: '+path
for path,digest in initial.items():assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest,'Source changed: '+path
(out/'receipt.json').write_text(json.dumps({'results':results,'sanitize':a.sanitize,'app_sha256':hashlib.sha256((obj/'default.elf').read_bytes()).hexdigest(),'actual_compiled_dependencies':actual_dependencies,'initial_source_sha256':initial,'source_sha256':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(paths) if p.is_file()},'scope':'Real Runtime/app/inventory/scene and native navigation; doubled update/WiFi/physical endpoints, no physical or real installation claim'},indent=2)+'\n')
