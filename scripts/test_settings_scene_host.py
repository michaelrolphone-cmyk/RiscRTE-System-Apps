#!/usr/bin/env python3
"""Qualify real Settings documents with the published provider, never a scene double.
Return nonzero on unmet round clipping, retaining all machine-readable evidence.
Explicit --round-side must be taken from the selected product's source metadata.
"""
import argparse,hashlib,json,math,os,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--runtime-sdk',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--round-side',type=int,required=True);p.add_argument('--round-inset',type=int,default=0);p.add_argument('--round-profile-source');p.add_argument('--sanitize',action='store_true');p.add_argument('--quick',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='settings-scene-host-') as tmp:
 inc=Path(tmp)/'include';shutil.copytree(ROOT/'lib/PortableApps/include',inc);shutil.copytree(ROOT/'lib/PortableApps/time',Path(tmp)/'time')
 for folder in [ROOT/'sdk/app',ROOT/'sdk/driver',a.runtime_sdk]:
  for h in folder.glob('*.h'):shutil.copyfile(h,inc/h.name)
 for h in [ROOT/'Services/scene_profile/SceneProfileV1.h',ROOT/'Services/text_input/SceneKeyboardV1.h']:shutil.copyfile(h,inc/h.name)
 exe=a.output/'settings-scene-host-test';flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-DPORTABLE_PREFERENCE_ANY_STORE','-I'+str(inc)]
 if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
 sources=[ROOT/'Services/scene_host/host.c',ROOT/'test/native_apps/settings_scene_host_test.c',*[ROOT/f'lib/PortableApps/src/{n}.c' for n in ['PortableSettingsModel','PortableSettingsScene','PortableTimeZone','PortableTimeZoneCatalog','PortableTimeZonePreference','PortableSetTime']]]
 tracked=sorted(set(sources)|set((ROOT/'Services/scene_host').rglob('*.inc'))|{ROOT/'test/scene/host_test.c'})
 source_hashes={str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in tracked}
 command=[os.environ.get('CC','cc'),*flags,*map(str,sources),'-Wl,--gc-sections','-o',str(exe)];subprocess.run(command,check=True)
 lanes=[('watch',240,240,5,0,0,0),('eink',800,480,1,270,0,0),('badge',a.round_side,a.round_side,5,0,a.round_inset,1)]
 # Generalize the display's conservative inscribed-square safe viewport.
 # No device-specific geometry enters the application or semantic document.
 lanes += [(f'general-{side}',side,side,5,0,(side-math.isqrt(side*side//2)+1)//2,1) for side in (300,330,400,480)]
 results=[]
 for label,*args in lanes:
  frame_dir=(a.output/(label+'-frames')).resolve();frame_dir.mkdir(exist_ok=True)
  r=subprocess.run([str(exe),*map(str,args),str(int(not a.quick))],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':os.environ.get('ASAN_OPTIONS','detect_leaks=0'),'SETTINGS_SCENE_FRAME_DIR':str(frame_dir)});(a.output/(label+'.log')).write_text(r.stdout+r.stderr)
  lines=[line for line in r.stdout.splitlines() if line.startswith('{')]
  result=json.loads(lines[-1]) if lines else {'native_raster_and_navigation':False,'error':r.stderr[-2000:]}
  result.update(label=label,returncode=r.returncode,sanitized=a.sanitize);results.append(result);print(json.dumps(result),flush=True)
 assert source_hashes=={str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in tracked},'Source changed during qualification'
 hashes={r.get('document_hash') for r in results if r.get('native_raster_and_navigation')}
 assert len(hashes)==1,'Device-dependent semantic documents: '+repr(hashes)
 (a.output/'receipt.json').write_text(json.dumps({'physical_acceptance':False,'actual_scene_host':True,'round_profile_source':a.round_profile_source,'round_insets_qualification':'Conservative declared safe square, not measured visible bounds or chord-aware layout','driver_pixel_rotation':'Hardware driver transform not exercised by software surface fixture','command':command,'source_sha256':source_hashes,'sdk_sha256':{f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(inc.glob('*.h'))},'results':results},indent=2)+'\n')
 raise SystemExit(any(r['returncode'] for r in results))
