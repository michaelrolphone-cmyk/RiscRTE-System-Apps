#!/usr/bin/env python3
"""Source-only native Serial Monitor UI qualification; no hardware or publication."""
import argparse, hashlib, json, os, shutil, subprocess
from pathlib import Path
from test_portable_serial_client import RUNTIME, HEADERS
ROOT=Path(__file__).resolve().parents[1]
CASES='terminal pagination no-selection inventory-failure back-before-open open-denied switch-instance repeat-tap happy repeat held refresh-failure snapshot-binding back-reselect poll-failure disconnect open-retained open-malformed call-retained read-retained read-malformed close-retained release-retained stale-invocation'.split()
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
 a.runtime=a.runtime.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);stage=out/'include'
 assert subprocess.check_output(['git','-C',a.runtime,'rev-parse','HEAD'],text=True).strip()==RUNTIME
 assert not subprocess.check_output(['git','-C',a.runtime,'status','--porcelain'],text=True).strip()
 shutil.copytree(ROOT/'lib/PortableApps/include',stage,dirs_exist_ok=True)
 canonical={}
 for group,names in {**HEADERS,'app':HEADERS['app']+['RiscRealtimeV1.h']}.items():
  for name in names:
   path='sdk/'+group+'/'+name;data=subprocess.check_output(['git','-C',a.runtime,'show',RUNTIME+':'+path]);assert data==(a.runtime/path).read_bytes();(stage/name).write_bytes(data);canonical[path]=hashlib.sha256(data).hexdigest()
 flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(stage),'-I'+str(ROOT/'lib/NativeApps/include')]
 flags+=['-D'+name for name in ['PORTABLE_SERIAL_STREAMS','PORTABLE_SERIAL_MONITOR_APP','PORTABLE_NATIVE_CUSTODY_FENCE','PORTABLE_CHECKED_STORAGE_RUNTIME','PORTABLE_APP_LAUNCH_GUARD','PORTABLE_APP_OWNS_TOUCH_CHROME']]
 if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 commands=[];raster=[]
 adapter_cases='render open-refused slow-frame retained-open retained-tick retained-close fini-clean fini-retained launch-clean launch-retained'.split()
 with (out/'tests.log').open('w') as log:
  def run(cmd):
   cmd=list(map(str,cmd));commands.append(cmd);log.write('$ '+' '.join(cmd)+'\n');log.flush();subprocess.run(cmd,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,env=env,check=True,timeout=60)
  cc=os.environ.get('CC','cc');client=ROOT/'lib/PortableApps/src/PortableSerialClient.c'
  run([cc,*flags,ROOT/'test/native_apps/serial_monitor_test.c',client,'-o',out/'controller'])
  for case in CASES:run([out/'controller',case])
  run([cc,*flags,'-DSM_TEST_INVALID_CATALOG',ROOT/'test/native_apps/serial_monitor_test.c',client,'-o',out/'invalid-catalog'])
  run([out/'invalid-catalog','invalid-catalog'])
  # Compile the unchanged client test too, so adapter additions cannot hide a
  # regression in the previously qualified contract.
  run([cc,*flags,ROOT/'test/native_apps/portable_serial_client_test.c',client,'-o',out/'client'])
  run([out/'client'])
  # Refuse accidental composition without its owner/fence, or with an
  # unqualified product navigation/sleep/resident policy.
  negative=[]
  for name in ['PORTABLE_NATIVE_CUSTODY_FENCE','PORTABLE_CHECKED_STORAGE_RUNTIME','PORTABLE_APP_LAUNCH_GUARD','PORTABLE_APP_OWNS_TOUCH_CHROME']:
   negative.append((name,[f for f in flags if f!='-D'+name]))
  for name in ['PORTABLE_HOME_APP','PORTABLE_RETURN_APP','PORTABLE_APP_SLEEP_LOCAL','PORTABLE_RESIDENT_SHELL_CLIENT','PORTABLE_QUICK_ACTIONS']:
   negative.append((name,[*flags,'-D'+name]))
  for name,negative_flags in negative:
   cmd=[cc,*negative_flags,'-fsyntax-only',ROOT/'test/native_apps/serial_monitor_adapter_test.c']
   commands.append(list(map(str,cmd)));result=subprocess.run(list(map(str,cmd)),cwd=ROOT,capture_output=True,text=True,env=env,timeout=60)
   assert result.returncode and 'Serial Monitor' in result.stderr,name
   log.write('Rejected unqualified composition: '+name+'\n')

  run([cc,*flags,ROOT/'test/native_apps/serial_monitor_adapter_test.c',ROOT/'Apps/serial_monitor.c',client,'-o',out/'adapter'])
  captures=out/'captures';captures.mkdir(exist_ok=True)
  for width,height in [(240,240),(480,800)]:
   for case in adapter_cases:run([out/'adapter',case,width,height,captures])
  from PIL import Image, ImageDraw
  selected=[]
  for path in sorted(captures.glob('*.ppm')):
   with Image.open(path) as source:
    im=source.convert('RGB');assert im.getextrema()!=((255,255),)*3
    dst=path.with_suffix('.png');im.save(dst)
    raster.append({'name':dst.name,'sha256':sha(dst),'width':im.width,'height':im.height})
    if path.name.startswith('render-'):selected.append((path.stem,im.copy()))
  sheet=Image.new('RGB',(4*260,((len(selected)+3)//4)*440),'#dddddd');draw=ImageDraw.Draw(sheet)
  for i,(title,im) in enumerate(selected):
   x=(i%4)*260;y=(i//4)*440;im.thumbnail((240,400));sheet.paste(im,(x+10,y+30));draw.text((x+5,y+5),title,fill='black')
  sheet.save(out/'contact-sheet.png')
 source=['Apps/serial_monitor.c','Apps/serial_monitor_font.inc','lib/PortableApps/include/PortableSerialMonitor.h','lib/PortableApps/include/PortableNativeCustody.h','lib/PortableApps/src/adapter.c','lib/PortableApps/src/native_custody_adapter.inc','lib/PortableApps/src/PortableSerialClient.c','test/native_apps/serial_monitor_test.c','scripts/test_serial_monitor.py','scripts/generate_serial_monitor_font.py','test/native_apps/serial_monitor_adapter_test.c','lib/PortableApps/paper_fonts/LICENSE-Rajdhani.txt','lib/PortableApps/quick_fonts/Rajdhani-600.ttf']
 receipt={'result':'PASS','runtime':RUNTIME,'system_parent':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'system_status':subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).splitlines(),'sanitized':a.sanitize,'sanitizer_environment':{k:env[k] for k in ['ASAN_OPTIONS','UBSAN_OPTIONS']},'cases':CASES+['invalid-catalog'],'negative_composition_controls':[n for n,_ in negative],'native_adapter_cases':adapter_cases,'native_adapter_dimensions':[[240,240],[480,800]],'captures':raster,'contact_sheet_sha256':sha(out/'contact-sheet.png'),'canonical_headers':canonical,'source_hashes':{p:sha(ROOT/p) for p in source},'commands':commands,'log_sha256':sha(out/'tests.log'),'not_claimed':['LeakSanitizer','full Xtensa/IDF/ELF build','native product admission','discovery/role scheduling','hardware','remote publication']}
 (out/'evidence.json').write_text(json.dumps(receipt,indent=2)+'\n');print('Serial Monitor UI PASS:',out)
if __name__=='__main__':main()
