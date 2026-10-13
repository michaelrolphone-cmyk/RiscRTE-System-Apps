#!/usr/bin/env python3
"""One shared executable accepts native inventory files and display dimensions.

The native authority's directory admission is qualified by Runtime tests. These
fixtures prove the real shared adapter consumes that publication, never linked
product data, while preserving all 23-entry controller/pixel behavior.
"""
import hashlib,json,os,re,subprocess,tempfile,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/runtime-app-catalog';OUT.mkdir(parents=True,exist_ok=True)
BASELINE='2c3f44dd773e27e3a142462088a96328119a79e7'
CC=os.environ.get('CC','cc')
common=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_NOVA_UI',
 '-DPORTABLE_WATCH_NATIVE_HEIGHT','-DPORTABLE_FORCE_FULL_FRAMES','-DPORTABLE_RETAINED_RGB565_HANDOFF',
 '-DPORTABLE_HANDOFF_EAGER_MS=60','-DPORTABLE_ASYNC_RGB565','-DPORTABLE_CATALOG_LIMIT=23',
 '-DPORTABLE_APP_LAUNCH_GUARD',
 '-DPORTABLE_RETURN_APP="clock.elf"']
includes=['-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),'-I'+str(ROOT/'lib/PortableApps/src')]
fixture=ROOT/'test/native_apps/fixtures/watch_springboard_catalog_23.c'
entries=re.findall(r'\.display_name="([^"]+)",\.file_name="([^"]+)",\.icon="([^"]+)",\.compatible=(true|false)',fixture.read_text())
assert len(entries)==23
results=[]
parity=[]
env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'}
with tempfile.TemporaryDirectory(prefix='runtime-app-catalog-') as directory:
 temp=Path(directory)
 def inventory(name,rows):
  path=temp/(name+'.tsv');path.write_text(''.join('\t'.join((*row[:3],'1' if row[3]=='true' else '0',*row[4:]))+'\n' for row in rows));return path
 files=[inventory('empty',[]),inventory('one',entries[:1]),inventory('delivered23',entries),
        inventory('different',[('Installed extra','Apps/extra/extra.elf','solid:f017','true'),('Unavailable','offline.elf','solid:f013','false')]),
        inventory('maximum128',[(f'Installed {i}',f'Apps/pkg{i}/app.elf','solid:f017','true') for i in range(128)]),
        inventory('long-route',[('Long installed app','Apps/'+('package-'*20)+'/app.elf','solid:f017','true')]),
        inventory('identity-collision',[('Short route','settings.elf','solid:f013','true'),('Long route','Apps/'+('package-'*20)+'/app.elf','solid:f017','true','settings.elf')])]
 def run(binary,args,label):
  result=subprocess.run([str(binary),*map(str,args)],env=env,capture_output=True,text=True,timeout=70)
  if result.returncode:raise RuntimeError(label+'\n'+result.stdout+result.stderr)
  results.append(label);print(label+' PASS',flush=True)
 def frames(path,label):
  hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in path.glob('*.ppm')}
  archive=OUT/(label+'.zip')
  with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as output:
   for p in path.glob('*.ppm'):output.write(p,p.name)
  with zipfile.ZipFile(archive) as saved:
   assert hashes=={name:hashlib.sha256(saved.read(name)).hexdigest() for name in saved.namelist()}
  for p in path.glob('*.ppm'):p.unlink()
  return hashes
 for sanitizer in (False,True):
  san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitizer else []
  binary=temp/'shared'
  subprocess.run([CC,*common,*san,*includes,str(ROOT/'test/native_apps/runtime_app_catalog_test.c'),
   str(ROOT/'lib/PortableApps/src/adapter.c'),str(fixture),'-o',str(binary)],check=True)
  shared_hash=hashlib.sha256(binary.read_bytes()).hexdigest()
  for width,height in ((240,240),(410,502)):
   for inv in files:run(binary,['inventory',inv,width,height],f'{inv.stem}-{width}x{height}-san{int(sanitizer)}')
   run(binary,['controller',files[2],width,height],f'nova-stale-controller-{width}x{height}-san{int(sanitizer)}')
   for fault in range(1,27):run(binary,['inventory',files[3],width,height,fault],f'fault{fault}-{width}x{height}-san{int(sanitizer)}')
  assert hashlib.sha256(binary.read_bytes()).hexdigest()==shared_hash
  legacy=temp/'legacy';legacy.mkdir(exist_ok=True)
  (legacy/'adapter.c').write_bytes(subprocess.check_output(['git','show',BASELINE+':lib/PortableApps/src/adapter.c'],cwd=ROOT))
  (legacy/'PortableApps.h').write_bytes(subprocess.check_output(['git','show',BASELINE+':lib/PortableApps/include/PortableApps.h'],cwd=ROOT))
  (legacy/'Apps').mkdir(exist_ok=True)
  for path in subprocess.check_output(['git','ls-tree','-r','--name-only',BASELINE,'Apps'],cwd=ROOT,text=True).splitlines():
   if Path(path).name.startswith('springboard') or Path(path).name=='SpringboardPresentation.h':
    (legacy/path).write_bytes(subprocess.check_output(['git','show',BASELINE+':'+path],cwd=ROOT))
  old=temp/'old'
  subprocess.run([CC,*[flag for flag in common if flag!='-DPORTABLE_APP_LAUNCH_GUARD'],*san,'-DSPRINGBOARD_SOURCE="'+str(legacy/'Apps/springboard.c')+'"','-I'+str(legacy),*includes,
   str(ROOT/'test/native_apps/watch_native_springboard_test.c'),str(legacy/'adapter.c'),str(fixture),'-o',str(old)],check=True)
  captures=temp/'captures';captures.mkdir(exist_ok=True)
  for width,height in ((240,240),(410,502)):
   for provider in ('sync','async'):
    run(old,[width,height,captures,provider],f'baseline-{width}x{height}-{provider}-san{int(sanitizer)}')
    expected=frames(captures,f'baseline-{width}x{height}-{provider}-san{int(sanitizer)}')
    run(binary,['parity',files[2],width,height,captures,provider],f'runtime-pixels-{width}x{height}-{provider}-san{int(sanitizer)}')
    actual=frames(captures,f'runtime-{width}x{height}-{provider}-san{int(sanitizer)}');assert actual==expected,(actual,expected)
    parity.append({'width':width,'height':height,'provider':provider,'sanitized':sanitizer,'frames':actual})
  # The adapter itself must have no compiled inventory symbol dependency.
  obj=temp/'adapter.o';subprocess.run([CC,*common,*includes,'-c',str(ROOT/'lib/PortableApps/src/adapter.c'),'-o',str(obj)],check=True)
  symbols=subprocess.check_output(['nm','-u',str(obj)],text=True)
  assert not re.search(r'\bportable_catalog(?:_count)?\b',symbols)
report={'passed':len(results),'checks':results,'geometry':[[240,240],[410,502]],
 'same_executable_different_inventories':True,'frame_parity':parity,'compiled_inventory_dependency':False,
 'baseline':BASELINE,'full_23_entry_pixels_identical':True,'asan_ubsan':True,
 'native_directory_admission':'Separate Runtime tests required','hardware_qualified':False}
(OUT/'result.json').write_text(json.dumps(report,indent=2)+'\n')
print(f'PASS {len(results)} cases: runtime inventories, 128-entry bound, stale launch fencing, custody faults and unchanged 23-entry pixels')
