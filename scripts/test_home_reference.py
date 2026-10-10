#!/usr/bin/env python3
"""Actual Home reference raster, retained-scene isolation, and app hit custody."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for key in ['utilities','runtime-sdk','driver-sdk','sleep-source','output']:p.add_argument('--'+key,type=Path,required=True)
p.add_argument('--utilities-revision',default='HEAD',help='Exact selected service source revision (resolved and recorded)')
p.add_argument('--reference',type=Path,default=ROOT/'test/native_apps/fixtures/home-refined-reference.png')
a=p.parse_args();out=a.output.resolve();include=out/'include';include.mkdir(parents=True,exist_ok=True)
shutil.copytree(ROOT/'lib/PortableApps/include',include,dirs_exist_ok=True);shutil.copytree(ROOT/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
for name in ('RiscRuntimeV1.h','RiscRealtimeV1.h','RiscProviderPromotionV1.h','RiscRetainedWakeV1.h'):shutil.copyfile(a.runtime_sdk/name,include/name)
for name in ('RiscDisplayOutputV1.h','RiscDisplayOutputPowerV1.h','RiscTouchV1.h','RiscTouchPowerV1.h','RiscStorageVolumeV1.h'):shutil.copyfile(a.driver_sdk/name,include/name)
pin=subprocess.check_output(['git','-C',a.utilities,'rev-parse',a.utilities_revision],text=True).strip()
for name in ('AlarmRecords.h','PointsRecords.h','PointsSchedule.h','PointsUtcSchedule.h','PointsCatalogProjection.h'):
 (include/name).write_bytes(subprocess.check_output(['git','-C',a.utilities,'show',pin+':lib/Alarm/include/'+name]))
base=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror']
incs=['-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include'),'-I'+str(a.runtime_sdk.parent/'driver'),'-I'+str(a.sleep_source.parent.parent/'drivers/x4pro_power')]
flags=['-DPORTABLE_RTC_WALL_TIME','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_CROWN_SLEEP_LOCAL','-DPORTABLE_SLEEP_MANUAL_ONLY','-DPORTABLE_DESK_CLOCK','-DPORTABLE_DESK_CLOCK_SPARSE_START','-DPORTABLE_HOME_POINTS_NATIVE_UTC','-DALARM_NATIVE_UTC']
names=['adapter.c','desk_clock_faces.c','PortableRealtimeClient.c','PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c']
sources=[ROOT/'Apps/paper_clock.c',*[ROOT/'lib/PortableApps/src'/n for n in names],a.sleep_source,ROOT/'test/native_apps/sparse_clock_startup_test.c']
quick=[ROOT/'lib/PortableApps/src'/n for n in ['quick_actions.c','quick_render.c','quick_session.c','quick_radios.c']]
cases=0
for san in [False,True]:
 directory=out/('sanitized' if san else 'normal');directory.mkdir(exist_ok=True)
 extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
 env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'}
 binary=directory/'render'
 subprocess.run([*base,*extra,*flags,'-DPORTABLE_DESK_POINTS_SNAPSHOT',*incs,'-ffunction-sections','-fdata-sections','-Wl,--gc-sections',str(ROOT/'test/native_apps/home_reference_render_test.c'),'-o',str(binary)],check=True)
 subprocess.run([binary,directory],check=True,env=env)
 for qa in [False,True]:
  binary=directory/f'controller-{int(qa)}'
  qflags=['-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_QUICK_RADIOS'] if qa else []
  subprocess.run([*base,*extra,*flags,'-DTEST_NATIVE_LANDSCAPE','-DPORTABLE_DISPLAY_ROTATION=90','-DPORTABLE_APP_OWNS_TOUCH_CHROME',*qflags,*incs,*map(str,sources),*map(str,quick if qa else []),'-o',str(binary)],check=True)
  def run(case,**values):
   global cases
   state=directory/'state';state.unlink(missing_ok=True)
   result=subprocess.run([binary,case,state],env={**env,'CLOCK_EPOCH':'1791369719' if case=='home-minute' else '1791369720',**values},capture_output=True,text=True,timeout=20)
   if result.returncode:raise RuntimeError(case+' '+str(values)+'\n'+result.stdout+result.stderr)
   cases+=1
  for case in ['home-ready','home-tap','home-dial','home-swipe','home-cancel','home-held','home-retry','home-minute','home-empty','home-invalid','home-unavailable']+(['home-top','home-top-left','home-top-held','home-top-cancel'] if qa else []):run(case)
  for i,name in enumerate(['file_browser.elf','points_in_time.elf','contexts.elf','settings.elf']):
   coords={'HOME_TAP_X':str(84+i*104),'HOME_TAP_Y':'690'}
   for asynchronous in ['0','1']:
    run('home-tap',**coords,HOME_EXPECT_LAUNCH=name,HOME_EXPECT_TARGET=str(4+i),RAW_ASYNC=asynchronous)
    for case in ['home-swipe','home-cancel','home-held']:run(case,**coords,RAW_ASYNC=asynchronous)
  print(f'Home actual controller: sanitized={san}, Quick Actions={qa}, dock taps/drag/cancel/held and async PASS',flush=True)
 for image in directory.glob('*.pbm'):Image.open(image).save(image.with_suffix('.png'))
for image in (out/'normal').glob('*.pbm'):assert image.read_bytes()==(out/'sanitized'/image.name).read_bytes(),image
reference=Image.open(a.reference).convert('L').point(lambda p:255 if p>=128 else 0).convert('1')
actual=Image.open(out/'normal/home-reference-24h.png').convert('1')
regions={'header':(24,18,456,64),'time':(24,72,456,202),'next':(24,254,456,308),'title':(24,312,456,376),'countdown':(24,382,456,412),'progress':(24,418,456,456),'following':(24,474,456,610),'dock':(24,620,456,774)}
scores={}
for name,box in regions.items():
 a_bits=[p==0 for p in actual.crop(box).getdata()];r_bits=[p==0 for p in reference.crop(box).getdata()]
 scores[name]=round(sum(x and y for x,y in zip(a_bits,r_bits))/sum(x or y for x,y in zip(a_bits,r_bits)),4)
 assert scores[name]>=0.75,(name,scores[name])
canvas=Image.new('RGB',(1000,858),'#dddddd');draw=ImageDraw.Draw(canvas)
font=ImageFont.truetype(str(ROOT/'lib/PortableApps/home_fonts/Rajdhani-700.ttf'),24)
draw.text((12,8),'Supplied reference',font=font,fill='black');draw.text((512,8),'Actual one-bit Home',font=font,fill='black')
canvas.paste(reference.convert('RGB'),(10,48));canvas.paste(actual.convert('RGB'),(510,48));canvas.save(out/'reference-comparison.png')
(out/'receipt.json').write_text(json.dumps({'source':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True)),'utilities_commit':pin,'controller_cases':cases,'pixel_identity':'normal and ASan/UBSan','reference':str(a.reference),'reference_sha256':hashlib.sha256(a.reference.read_bytes()).hexdigest(),'region_black_pixel_iou':scores,'limits':['Hardware not run.','Status icons require admitted cached status; unknown states are not painted.'],'sources':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [ROOT/'Apps/paper_clock.c',ROOT/'Apps/paper_home_reference.inc',ROOT/'Apps/paper_home_type.inc',ROOT/'Apps/paper_home_points.inc',ROOT/'lib/PortableApps/home_fonts/reference.inc']}},indent=2)+'\n')
print(f'PASS {cases} controller cases; reference IoU {scores}')
