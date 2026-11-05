#!/usr/bin/env python3
"""Paper Quick coordinate contract, actual adapter/provider-pixel regressions.

Builds sequentially. Retains scripts, receipts and captures; hashes/deletes only
executables created by this runner. No firmware publication or hardware claim.
"""
import argparse, hashlib, json, os, shlex, shutil, subprocess
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
PARSER=argparse.ArgumentParser(description=__doc__)
PARSER.add_argument('--runtime-sdk',type=Path,required=True)
PARSER.add_argument('--display-sdk',type=Path,required=True)
PARSER.add_argument('--output',type=Path,default=ROOT/'build/paper-coordinate-adapter')
PARSER.add_argument('--baseline-dir',type=Path,default=ROOT/'docs/paper-coordinate-geometry',help='Frozen 480×800 reference PNGs and metadata')
ARGS=PARSER.parse_args();OUT=ARGS.output.resolve();OUT.mkdir(parents=True,exist_ok=True)
BASELINE=json.loads((ARGS.baseline_dir/'baseline.json').read_text())
for name in ('legacy-480x800.png','resident-480x800-no-audio.png','resident-480x800-audio.png'):
 assert (ARGS.baseline_dir/name).is_file(), 'Missing baseline PNG: '+name
 assert hashlib.sha256((ARGS.baseline_dir/name).read_bytes()).hexdigest()==BASELINE['png_sha256'][name], 'Baseline PNG checksum mismatch: '+name
INCLUDE=OUT/'include';shutil.copytree(ROOT/'lib/PortableApps/include',INCLUDE,dirs_exist_ok=True)
shutil.copytree(ROOT/'lib/PortableApps/time',OUT/'time',dirs_exist_ok=True)
SDK={};SDK_STAGED=[]
for folder,names in [(ARGS.runtime_sdk,['RiscRuntimeV1.h','RiscRuntimeHostControlV1.h','RiscResidentShellV1.h','RiscFailureEvidenceV1.h']), (ARGS.display_sdk,['RiscDisplayOutputV1.h','RiscDisplayOutputPowerV1.h','RiscDisplayOutputMetricsV1.h','RiscDisplayOutputSnapshotV1.h'])]:
 for name in names:
  p=folder/name;SDK[str(p.resolve())]=hashlib.sha256(p.read_bytes()).hexdigest();shutil.copyfile(p,INCLUDE/name)
  staged=INCLUDE/name;staged_hash=hashlib.sha256(staged.read_bytes()).hexdigest()
  assert staged_hash==SDK[str(p.resolve())], 'SDK changed while staging: '+str(p)
  SDK_STAGED.append({'source':str(p.resolve()),'staged':str(staged),'sha256':staged_hash})
RESULTS=[];CLOSURE=[];HASHES={};COMPARE={};ENV=dict(os.environ,ASAN_OPTIONS='detect_leaks=0')
SAN=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
COMMON=['-DPORTABLE_ALARM_CLIENT','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_APP_OWNS_TOUCH_CHROME']
QUICK=[ROOT/'lib/PortableApps/src'/n for n in ('quick_actions.c','quick_render.c','quick_session.c')]
SOURCE_INPUTS=[Path(__file__).resolve(),ROOT/'.github/workflows/paper-coordinate-geometry.yml',*[ROOT/'test/native_apps'/name for name in ['paper_coordinate_clock_fixture.c','paper_coordinate_quick_fixture.c','paper_coordinate_test.c','paper_coordinate_resident_test.c']]]
for source in SOURCE_INPUTS:
 if not source.is_file():raise RuntimeError('Required source receipt input is missing: '+str(source))
SOURCE_HASHES={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in SOURCE_INPUTS}
LOG=(OUT/'test.log').open('a')
def build(target,sources,flags,includes=INCLUDE,shared=False):
 cmd=['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-I'+str(includes),'-I'+str(ROOT/'lib/NativeApps/include'),*flags,*map(str,sources)]
 if shared:cmd+=['-fPIC']
 dep=subprocess.run(cmd+['-MM'],check=True,capture_output=True,text=True).stdout
 (OUT/(target.relative_to(OUT).as_posix().replace('/','_')+'.dependencies.mk')).write_text(dep)
 files=[]
 for token in shlex.split(dep.replace('\\\n',' ')):
  if token.endswith(':'):continue
  dependency=Path(token).resolve()
  if not dependency.is_file():raise RuntimeError('Compiler-listed dependency is missing: '+str(dependency))
  files.append(str(dependency))
 files=sorted(set(files))
 CLOSURE.append({'target':str(target),'command':cmd,'sha256':{p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in files}})
 cmd=[a for a in cmd if a!='-ldl']+(['-ldl'] if '-ldl' in cmd else [])
 cmd+=(['-fPIC','-shared','-Wl,-Bsymbolic'] if shared else ['-no-pie'])+['-o',str(target)]
 LOG.write('BUILD '+' '.join(cmd)+'\n');LOG.flush();subprocess.run(cmd,check=True,stdout=LOG,stderr=LOG)
def run(binary,*args):
 r=subprocess.run([str(binary),*map(str,args)],check=False,timeout=30,text=True,capture_output=True,env=ENV)
 LOG.write(r.stdout+r.stderr);LOG.flush();RESULTS.append({'binary':str(binary),'args':list(map(str,args)),'stdout':r.stdout,'returncode':r.returncode})
 r.check_returncode()
def remove(binary):
 HASHES[str(binary)]=hashlib.sha256(binary.read_bytes()).hexdigest();binary.unlink()
def image(p):return Image.open(p).convert('1')
def equal(name,a,b):
 a=image(a);b=image(b);COMPARE[name]=a.size==b.size and a.tobytes()==b.tobytes();assert COMPARE[name],name
def dims(w,h,lw,lh):return [f'-DTEST_PAPER_WIDTH={w}',f'-DTEST_PAPER_HEIGHT={h}',f'-DTEST_PAPER_LOGICAL_WIDTH={lw}',f'-DTEST_PAPER_LOGICAL_HEIGHT={lh}']
# Provider coordinates are logical; display90 and raw-touch180 are distinct.
configs=[('legacy480',480,800,480,800,[]),('legacy410',410,700,410,700,[]),('rotate480',800,480,480,800,['-DPORTABLE_DISPLAY_ROTATION=90']),('flip480',480,800,480,800,['-DPORTABLE_PAPER_PREFERENCES','-DTEST_READER_FLIP']),('snapshot410',410,700,410,700,['-DPORTABLE_RASTER_SNAPSHOT']),('rotate410',700,410,410,700,['-DPORTABLE_DISPLAY_ROTATION=90']),('flip410',410,700,410,700,['-DPORTABLE_PAPER_PREFERENCES','-DTEST_READER_FLIP']),('rawrotate410',410,700,410,700,['-DPORTABLE_TOUCH_ROTATION=180','-DTEST_RAW_TOUCH_ROTATION']),('sanitized410',410,700,410,700,SAN)]
for name,w,h,lw,lh,flags in configs:
 binary=OUT/(name+'.bin');build(binary,[ROOT/'Apps/paper_clock.c',ROOT/'test/native_apps/paper_coordinate_test.c',ROOT/'lib/PortableApps/src/adapter.c',*QUICK],COMMON+['-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_RTC_WALL_TIME','-DPORTABLE_CROWN_SLEEP_UNAVAILABLE']+dims(w,h,lw,lh)+flags,ROOT/'lib/PortableApps/include')
 tests=[(26,300,0),(27,300,0),(28,300,2),(200,300,2),(201,300,2),(202,300,0),(100,263,0),(100,264,0),(100,265,2),(100,343,2),(100,344,2),(100,345,0)] if lw==410 else [(31,345,0),(32,345,2),(235,345,2),(236,345,0),(130,301,0),(130,302,2),(130,393,2),(130,394,0)]
 cap=OUT/(name+'-capture');cap.mkdir(exist_ok=True)
 for i,(x,y,writes) in enumerate(tests):run(binary,x,y,writes,*([cap] if i==0 else []))
 cancel=OUT/(name+'-cancel');cancel.mkdir(exist_ok=True);run(binary,100 if lw==410 else 130,300 if lw==410 else 345,0,cancel,1)
 if name in ('legacy410','snapshot410','sanitized410'):
  delayed=OUT/(name+'-delayed');delayed.mkdir(exist_ok=True);run(binary,100,300,2,delayed,2)
 remove(binary)
 for p in cap.glob('*.pbm'):
  im=image(p)
  if name.startswith('rotate'):im=im.rotate(270,expand=True)
  if name.startswith('flip'):im=im.rotate(180)
  im.save(p.with_suffix('.png'))
for name,w,h,lw,lh,flags in [('resident480',480,800,480,800,[]),('resident640',480,640,480,640,[]),('resident640-snapshot',480,640,480,640,['-DPORTABLE_RASTER_SNAPSHOT']),('resident480-rotate',800,480,480,800,['-DPORTABLE_DISPLAY_ROTATION=90']),('resident480-flip',480,800,480,800,['-DTEST_READER_FLIP']),('resident640-sanitize',480,640,480,640,SAN)]:
 out=OUT/name;out.mkdir(exist_ok=True);base=COMMON+['-DPORTABLE_PAPER_PREFERENCES','-DPORTABLE_ALARM_TERMINAL_RETENTION','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_HOME_APP="default.elf"']+dims(w,h,lw,lh)+flags
 for role in ('host','client'):
  extra=['-DPORTABLE_RESIDENT_SHELL_'+role.upper()]+(['-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_QUICK_USB_TRANSFER','-DPORTABLE_PAPER_TRANSITIONS'] if role=='host' else ['-DPORTABLE_APP_LAUNCH_GUARD','-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_CROWN_SLEEP_LOCAL'])
  build(out/(role+'.elf'),[ROOT/'test/native_apps/resident_shell_app.c',ROOT/'lib/PortableApps/src/adapter.c']+(QUICK if role=='host' else []),base+extra,shared=True)
 binary=out/'resident-test.bin';build(binary,[ROOT/'test/native_apps/paper_coordinate_resident_test.c'],dims(w,h,lw,lh)+flags+['-rdynamic','-ldl'])
 for mode in [0,1,3,5,7,20]:run(binary,out,mode)
 for mode in [0,7]:
  im=image(out/f'quick-{mode}.pbm')
  if 'rotate' in name:im=im.rotate(270,expand=True)
  if 'flip' in name:im=im.rotate(180)
  im.save(out/f'quick-{mode}.png')
 for p in (binary,out/'host.elf',out/'client.elf'):remove(p)
# Retain all original modal/launcher controls without modifying their assertions.
for launcher in [False,True]:
 binary=OUT/('original-launcher.bin' if launcher else 'original-clock.bin')
 flags=['-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_RTC_WALL_TIME','-DPORTABLE_INPUT_NAVIGATION']+(['-DTEST_SPRINGBOARD','-DPORTABLE_RETURN_APP="parent.elf"','-DPORTABLE_HOME_APP="default.elf"'] if launcher else ['-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_CROWN_SLEEP_UNAVAILABLE'])
 build(binary,[ROOT/'Apps'/('springboard.c' if launcher else 'paper_clock.c'),ROOT/'test/native_apps/paper_quick_test.c',ROOT/'lib/PortableApps/src/adapter.c',*QUICK],flags,ROOT/'lib/PortableApps/include')
 for case in (range(20,28) if launcher else [*range(19),30,31,32,33]):
  if not launcher and case==0:
   cap=OUT/'original480-capture';cap.mkdir(exist_ok=True);run(binary,case,cap)
  else:run(binary,case)
 remove(binary)
# Exact reference 480 equivalence and matching provider rasters at non-reference sizes.
for name in ['legacy480','rotate480','flip480']:
 equal(name+'-reference',OUT/f'{name}-capture/frame-2.png',ARGS.baseline_dir/'legacy-480x800.png')
for name in ['rotate480','flip480']:
 equal(name+'-normalized',OUT/f'{name}-capture/frame-2.png',OUT/'legacy480-capture/frame-2.png')
for name in ['snapshot410','rotate410','flip410','rawrotate410','sanitized410']:
 equal(name+'-normalized',OUT/f'{name}-capture/frame-2.png',OUT/'legacy410-capture/frame-2.png')
for name in ['legacy410','snapshot410','sanitized410']:
 equal(name+'-delayed',OUT/f'{name}-delayed/frame-2.pbm',OUT/'legacy410-capture/frame-2.png')
for mode in [0,7]:
 reference=image(ARGS.baseline_dir/('resident-480x800-audio.png' if mode==7 else 'resident-480x800-no-audio.png'))
 for name in ['resident480','resident480-rotate','resident480-flip']:
  equal(f'{name}-mode{mode}-reference',OUT/name/f'quick-{mode}.png',ARGS.baseline_dir/('resident-480x800-audio.png' if mode==7 else 'resident-480x800-no-audio.png'))
 expected=Image.new('1',(480,640),1)
 for y in range(640):expected.paste(reference.crop((0,y*800//640,480,y*800//640+1)),(0,y))
 actual=image(OUT/'resident640'/f'quick-{mode}.png')
 assert expected.tobytes()==actual.tobytes(),f'resident640 mode{mode} inverse-floor reference'
 COMPARE[f'resident640-mode{mode}-full-inverse-floor-reference']=True
 expected.save(OUT/f'resident640-expected-{mode}.png')
 for name in ['resident480-rotate','resident480-flip']:
  equal(f'{name}-mode{mode}-normalized',OUT/name/f'quick-{mode}.png',OUT/'resident480'/f'quick-{mode}.png')
 for name in ['resident640-snapshot','resident640-sanitize']:
  equal(f'{name}-mode{mode}',OUT/name/f'quick-{mode}.png',OUT/'resident640'/f'quick-{mode}.png')
equal('unchanged-original480',OUT/'original480-capture/frame-2.pbm',ARGS.baseline_dir/'legacy-480x800.png')
# The top/bottom painted spans and all outside edges are measured provider pixels.
frame=image(OUT/'legacy410-capture/frame-2.png')
for y in range(265,345):assert frame.getpixel((27,y))!=0 and frame.getpixel((202,y))!=0
for x in range(28,202):
 assert frame.getpixel((x,264))!=0 and frame.getpixel((x,345))!=0
 assert frame.getpixel((x,265))==0 and frame.getpixel((x,344))==0
assert len(RESULTS)==175 and len(COMPARE)==30
baseline_meta=BASELINE
(OUT/'receipt.json').write_text(json.dumps({'results':RESULTS,'comparisons':COMPARE,'source_closure':CLOSURE,'sdk':SDK,'sdk_staged_provenance':SDK_STAGED,'runner_workflow_fixture_sha256':SOURCE_HASHES,'executable_sha256':HASHES,'baseline':baseline_meta,'baseline_assets_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in ARGS.baseline_dir.glob('*.png')},'source_head':subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip(),'hardware_run':False},indent=2)+'\n')
print('175 production adapter/provider cases and 30 pixel/reference comparisons PASS; no executables retained')
