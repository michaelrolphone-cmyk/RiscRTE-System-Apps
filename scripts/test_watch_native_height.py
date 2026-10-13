#!/usr/bin/env python3
"""Bounded original-Watch native-height Settings/Quick/async qualification.

Uses temporary executables; stores only a JSON result, never a device image.
The golden 240 pixels come from exact delivered Settings 1.3.24 source.
"""
import hashlib,json,os,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/watch-native-height';OUT.mkdir(parents=True,exist_ok=True)
base=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
 '-DPORTABLE_NOVA_UI','-DPORTABLE_WATCH_NATIVE_HEIGHT',
 '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include')]
src=ROOT/'lib/PortableApps/src';tests=ROOT/'test/native_apps';app=ROOT/'Apps/settings.c'
quick=[src/(n+'.c') for n in ('quick_actions','quick_render','quick_session')]
golden=json.loads((tests/'watch_native_settings_golden.json').read_text())['frames']
runs=[];leak_limited=False
with tempfile.TemporaryDirectory(prefix='watch-native-height-') as tmp:
 tmp=Path(tmp);exe=tmp/'test';frames=tmp/'frames';frames.mkdir()
 def build(flags,sources):
  subprocess.run([*base,*flags,*map(str,sources),'-o',str(exe)],check=True,timeout=150)
 def run(label,args=()):
  global leak_limited
  env=os.environ.copy()
  if leak_limited:env['ASAN_OPTIONS']='detect_leaks=0'
  result=subprocess.run([str(exe),*map(str,args)],capture_output=True,text=True,timeout=40,env=env)
  if result.returncode and 'LeakSanitizer does not work under ptrace' in result.stderr:
   leak_limited=True;env['ASAN_OPTIONS']='detect_leaks=0'
   result=subprocess.run([str(exe),*map(str,args)],capture_output=True,text=True,timeout=40,env=env)
  if result.returncode:raise RuntimeError(label+'\n'+result.stdout+result.stderr)
  runs.append(label);print(label+' PASS',flush=True)
 for sanitized in (False,True):
  san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
  build(san,[tests/'watch_native_quick_test.c',*quick[:2]])
  run(f'quick-pixel-guard-san{int(sanitized)}')
  build([*san,'-DPORTABLE_SLEEP_SETTINGS'],[app,tests/'nova_settings_test.c'])
  for mode in (0,1):
   run(f'240-golden-{mode}-san{int(sanitized)}',[frames,mode])
   for p in frames.glob('*.ppm'):
    assert hashlib.sha256(p.read_bytes()).hexdigest()==golden[str(mode)+'/'+p.name],p
  for width,height in ((240,240),(410,502)):
   for rotation in (0,180):
    tag=f'{width}x{height}-r{rotation}-san{int(sanitized)}'
    flags=[*san,f'-DPORTABLE_TOUCH_ROTATION={rotation}',f'-DSETTINGS_FIXTURE_WIDTH={width}',f'-DSETTINGS_FIXTURE_HEIGHT={height}']
    build([*flags,'-DPORTABLE_SLEEP_SETTINGS','-DSETTINGS_FIXTURE_PADDING=5'],[app,tests/'watch_native_settings_test.c'])
    run('settings-'+tag)
    build([*flags,'-DPORTABLE_QUICK_RADIOS'],[app,tests/'quick_adapter_test.c',*quick,src/'quick_radios.c'])
    for case in range(13):run(f'quick-{tag}-{case}',[case])
    build([*flags,'-DPORTABLE_ASYNC_RGB565'],[app,tests/'watch_async_modal_test.c',*quick])
    for case in ('controls','controls-settings','controls-save','torch-alarm','alarm','crown-controls','crown-alarm','failure-controls','failure-alarm'):
     run(f'async-{tag}-{case}',[case])
report={'runs':len(runs),'cases':runs,'golden_source':'3508092f135d428b5bbccee0546616d046938527',
 'geometry':{'240x240':[240,240],'410x502':[240,294]},'asan_ubsan':True,
 'leak_sanitizer_ptrace_limited':leak_limited,'hardware_qualified':False}
(OUT/'result.json').write_text(json.dumps(report,indent=2)+'\n')
print(f'Native-height Watch Settings/Quick: {len(runs)} real-controller and surface cases passed; original 240 frames pixel-identical.')
