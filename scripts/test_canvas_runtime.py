#!/usr/bin/env python3
import pathlib,tempfile,sys,subprocess,json,hashlib,os,datetime,argparse
REPOSITORY=pathlib.Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description="Actual Runtime/ProviderGraph canvas lifecycle qualification")
parser.add_argument('--runtime',type=pathlib.Path,required=True)
parser.add_argument('--system',type=pathlib.Path,default=REPOSITORY)
parser.add_argument('--output',type=pathlib.Path,required=True)
parser.add_argument('--sanitize',action='store_true')
args=parser.parse_args()
ROOT=REPOSITORY/'test/canvas_runtime';SYSTEM=args.system.resolve();RUNTIME=args.runtime.resolve();CANVAS=REPOSITORY
OUTPUT=args.output.resolve();OUTPUT.mkdir(parents=True,exist_ok=False)
sys.path.insert(0,str(SYSTEM/'scripts'));from scene_sdk import stage_sdk
SANITIZED=args.sanitize
MODE="sanitized" if SANITIZED else "normal"
commands=[]
def run(command,env):
 commands.append(list(map(str,command)));(OUTPUT/('commands-'+MODE+'.json')).write_text(json.dumps(commands,indent=2)+'\n')
 result=subprocess.run(command,capture_output=True,text=True,env=env)
 if result.returncode:
  (OUTPUT/('FAILURE-'+MODE+'.json')).write_text(json.dumps({'command':commands[-1],'exit':result.returncode,'stdout':result.stdout,'stderr':result.stderr},indent=2)+'\n');raise RuntimeError(result.stderr)
 return result.stdout
with tempfile.TemporaryDirectory(prefix='canvas-native-runtime-') as temporary:
 out=pathlib.Path(temporary);include=stage_sdk(RUNTIME,SYSTEM,out/'sdk')
 flags=['-O1','-Wall','-Wextra','-Werror']+(['-fsanitize=address,undefined','-fno-omit-frame-pointer'] if SANITIZED else []);env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0');incs=['-I'+str(include),'-I'+str(CANVAS/'sdk/app')]
 def shared(source,name,defines=()):run(['cc','-std=c11',*flags,*incs,*defines,'-fPIC','-fvisibility=hidden','-shared',str(source),'-o',str(out/name)],env)
 shared(CANVAS/'Services/canvas_host/host.c','canvas.elf');shared(SYSTEM/'Services/scene_profile/profile.c','profile.elf');shared(ROOT/'provider.c','display.elf');shared(ROOT/'app.c','default.elf')
 def write(name,value):(out/name).write_text(json.dumps(value,indent=2)+'\n')
 def req(cap):return {'capability':cap,'api':1}
 def manifest(name,cap,requires):return {'type':'driver','id':{'canvas':'canvas-host','profile':'scene-presentation-profile'}.get(name,name),'version':'0.1.0','driver_abi':2,'architecture':'xtensa-esp32s3','file_name':name+'.elf','requires':[req(c) for c in requires],'provides':[req(cap)]}
 write('canvas.json',manifest('canvas','ui.canvas',['display.output','ui.presentation-profile']));write('profile.json',manifest('profile','ui.presentation-profile',[]));write('display.json',manifest('display','display.output',[]))
 write('board.json',{'schema':'riscrte.board-hardware','schema_version':1,'board_id':'test','revision':'unspecified','buses':[],'devices':[]})
 write('default.json',{'type':'application','id':'default','version':'0.1.0','architecture':'xtensa-esp32s3','file_name':'default.elf','entry':'app_main','requires':[req('ui.canvas')]})
 write('boot.json',{'board':'board.json','default_app':'default.elf','provider_activation':'eager','drivers':[{'manifest':name+'.json'} for name in ['canvas','profile','display']],'app_capabilities':[{'manifest':'default.json','grants':[{**req('ui.canvas'),'instance_id':0}]}]})
 sources=['bootstrap/Json.cpp','bootstrap/Board.cpp','bootstrap/Runtime.cpp','runtime/streams/AppStreamSessions.cpp','runtime/streams/ProviderQueueHost.cpp','runtime/drivers/ProviderGraphV2.cpp','runtime/drivers/ProviderModuleV2.cpp']
 files=[p for folder in ('src','sdk','lib/ArduinoJson/src','test/drivers/stubs') for p in (RUNTIME/folder).rglob('*') if p.is_file()]+[RUNTIME/'test/RuntimeFileFixture.h',SYSTEM/'scripts/scene_sdk.py']+list(ROOT.glob('*.c'))+list(ROOT.glob('*.cpp'))+list((CANVAS/'Services/canvas_host').glob('*'))+list((CANVAS/'Services/canvas_input').glob('*'))+[SYSTEM/'Services/scene_profile/profile.c',pathlib.Path(__file__)]+list(include.glob('*.h'))
 (OUTPUT/('SOURCE-IDENTITY-'+MODE+'.json')).write_text(json.dumps({str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in files},indent=2)+'\n')
 run(['c++','-std=c++17',*flags,'-Wno-missing-field-initializers','-O0','-fno-pie','-no-pie','-rdynamic',*incs,'-I'+str(RUNTIME/'src'),'-I'+str(RUNTIME/'test'),'-I'+str(RUNTIME/'lib/ArduinoJson/src'),'-I'+str(RUNTIME/'test/drivers/stubs'),*[str(RUNTIME/'src'/p) for p in sources],str(ROOT/'runtime_test.cpp'),'-ldl','-o',str(out/'test')],env)
 stdout=run([str(out/'test'),str(out)],env)
 # Same actual Runtime graph contains legacy input-capable Viewer Canvas AND
 # input-free Home raster. The TIMER-equivalent frames precede input startup.
 shared(CANVAS/'Services/canvas_host/host.c','raster.elf',['-DRISC_CANVAS_RASTER_PROVIDER'])
 shared(CANVAS/'Services/canvas_input/input.c','input.elf')
 shared(ROOT/'navigation.c','navigation.elf')
 shared(ROOT/'app.c','default.elf',['-DTEST_CANVAS_CAPABILITY=RISC_CANVAS_RASTER_CAPABILITY','-DTEST_SPLIT_INPUT'])
 raster_manifest=manifest('raster','ui.canvas.raster',['display.output','ui.presentation-profile']);raster_manifest['id']='canvas-raster'
 input_manifest=manifest('input','ui.canvas-input',['ui.canvas.raster','input.navigation']);input_manifest['id']='canvas-input'
 write('raster.json',raster_manifest);write('input.json',input_manifest)
 write('navigation.json',manifest('navigation','input.navigation',[]))
 write('canvas.json',manifest('canvas','ui.canvas',['display.output','ui.presentation-profile','input.navigation']))
 write('default.json',{'type':'application','id':'default','version':'0.1.0','architecture':'xtensa-esp32s3','file_name':'default.elf','entry':'app_main','requires':[req('ui.canvas.raster'),req('ui.canvas-input')]})
 write('boot.json',{'board':'board.json','default_app':'default.elf','provider_activation':'demand','drivers':[{'manifest':name+'.json'} for name in ['canvas','raster','input','navigation','profile','display']],'app_capabilities':[{'manifest':'default.json','grants':[{**req(c),'instance_id':0} for c in ['ui.canvas.raster','ui.canvas-input']]}]})
 stdout+=run([str(out/'test'),str(out)],env)
 stdout+='Legacy Canvas and separately named raster coexist; raster frames activate no input, explicit foreground companion owns navigation and checked no-touch sleep.\n'
 (OUTPUT/('RESULT-'+MODE+'.json')).write_text(json.dumps({'observedAt':datetime.datetime.now(datetime.timezone.utc).isoformat(),'stdout':stdout,'sanitized':SANITIZED,'binary_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in list(out.glob('*.elf'))+[out/'test']},'scope':'Actual Runtime/ProviderGraph host execution, modeled display, no target or physical acceptance.'},indent=2)+'\n');print(stdout)
