#!/usr/bin/env python3
"""Production Springboard/adapter scroll, identity, frame and interruption checks."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
from PIL import Image,ImageOps
ROOT=Path(__file__).resolve().parents[1]
CASES='empty one seventeen transition tap busy-tap drag queued-up bottom-tap bounds horizontal stop-tap busy-hit new-contact reorder insert remove cancelled home home-touch quick quick-pending flipped launch-fail superseded superseded-feedback'.split()
CASES += ['twelve','thirteen','dots','eighteen','twenty','twentyone','swipe-usb','gameboy']
CASES += ['clock-'+name for name in '12 24 midnight-12 midnight-24 noon-12 single-12 single-24 missing invalid unavailable'.split()]
CASES += ['swipe-'+name for name in 'fast reversed empty one seventeen left right bounds short threshold diagonal vertical-lock horizontal-lock cancelled new-id queued-up queued-short flipped home home-touch top quick busy busy-reverse new-contact tap last superseded'.split()]
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--catalog',type=Path,help='Capture the explicitly selected deployment catalog')
 p.add_argument('--runtime-sdk',type=Path,required=True);p.add_argument('--display-sdk',type=Path,required=True)
 p.add_argument('--snapshot',action='store_true',help='Exercise the deployed deferred renderer')
 p.add_argument('--output-dir',type=Path,default=ROOT/'build/touch-scroll-springboard');p.add_argument('--normal-only',action='store_true');p.add_argument('--case',action='append',choices=CASES)
 a=p.parse_args();out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
 include=out/'include';shutil.copytree(ROOT/'lib/PortableApps/include',include,dirs_exist_ok=True);shutil.copytree(ROOT/'lib/PortableApps/time',out/'time',dirs_exist_ok=True)
 for name in ('RiscRuntimeV1.h','RiscRealtimeV1.h'):shutil.copyfile(a.runtime_sdk/name,include/name)
 for name in ('RiscDisplayOutputV1.h','RiscDisplayOutputPowerV1.h','RiscDisplayOutputMetricsV1.h','RiscDisplayOutputSnapshotV1.h'):shutil.copyfile(a.display_sdk/name,include/name)
 img=Image.open(ROOT/'docs/evidence/home-parity/home-ready.png').convert('1').rotate(90,expand=True)
 source=out/'home.bin';source.write_bytes(ImageOps.invert(img.convert('L')).convert('1').tobytes())
 extra_sources=[];catalog_flags=['-DPORTABLE_SPRINGBOARD_CATALOG_BOUND=22']
 if a.catalog:
  from build_portable_springboard import catalog_source
  catalog=out/'selected-catalog.c';catalog.write_text(catalog_source(a.catalog,40)[0]);extra_sources=[str(catalog)];catalog_flags=['-DTEST_DEPLOYMENT_CATALOG','-DPORTABLE_SPRINGBOARD_CATALOG_BOUND='+str(max(1,len(json.loads(a.catalog.read_text())['apps'])))]
 results=[]
 for sanitized in ([False] if a.normal_only else [False,True]):
  binary=out/('scroll-sanitized' if sanitized else 'scroll')
  flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
  if a.snapshot:flags+=['-DPORTABLE_RASTER_SNAPSHOT']
  subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*catalog_flags,
   '-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DTEST_NATIVE_TOOLBAR_QUICK','-DPORTABLE_INPUT_NAVIGATION','-DPORTABLE_HOME_APP="default.elf"',
   '-DPORTABLE_PAPER_CROSSFADE','-DPORTABLE_PAPER_TRANSITIONS','-DPORTABLE_PAPER_PREFERENCES','-DPORTABLE_TOUCH_SCROLL','-DPORTABLE_APP_TOUCH_SCROLL','-DPORTABLE_SPRINGBOARD_TOUCH_SCROLL','-DPORTABLE_NOVA_UI',
   '-I'+str(include),'-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'test/native_apps/touch_scroll_springboard_test.c'),str(ROOT/'test/native_apps/touch_scroll_springboard_entry.c'),
   *[str(ROOT/'lib/PortableApps/src'/name) for name in ('quick_actions.c','quick_render.c','quick_session.c')],*extra_sources,'-Wl,--wrap=free','-Wl,--wrap=malloc','-o',str(binary)],check=True)
  model=out/('pages-model-sanitized' if sanitized else 'pages-model')
  subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',*flags,'-DPORTABLE_TOUCH_SCROLL','-I'+str(include),str(ROOT/'test/native_apps/springboard_pages_test.c'),'-o',str(model)],check=True)
  subprocess.run([model],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
  for case in a.case or CASES:
   directory=out/f'{case}-{int(sanitized)}';directory.mkdir(exist_ok=True)
   result=subprocess.run([binary,case,source,'-' if sanitized else directory],text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'),timeout=20)
   if result.returncode:raise AssertionError((case,sanitized,result.stdout,result.stderr))
   results.append(dict(sanitized=sanitized,**json.loads(result.stdout)));print(result.stdout.strip(),flush=True)
   if not sanitized:
    for file in directory.glob('*.pbm'):Image.open(file).rotate(270,expand=True).save(file.with_suffix('.png'))
 # The footer contains only page dots, with no numeric page-count label.
 for case in a.case or CASES:
  if case in ('launch-fail','flipped','swipe-flipped','quick','quick-pending','swipe-quick','superseded','superseded-feedback','swipe-superseded'):continue
  endpoint=out/f'{case}-0/endpoint.png'
  if endpoint.exists():
   assert Image.open(endpoint).convert('L').crop((352,746,448,786)).getextrema()==(255,255),case+' rendered a page-count label'
 # Clock-only rendering validates the requested header without exercising launch.
 # The 30px reference title and baseline are fixed; its middle field is empty.
 clock_headers=[]
 for case in a.case or CASES:
  if not case.startswith('clock-'):continue
  header=Image.open(out/f'{case}-0/endpoint.png').convert('L').crop((0,0,480,88))
  assert header.crop((160,18,320,74)).getextrema()==(255,255),case+' rendered extra header chrome'
  assert header.crop((32,78,448,82)).getextrema()==(0,0),case+' reference divider differs'
  assert header.crop((448,0,480,78)).getextrema()==(255,255),case+' clock overflows reference margin'
  title=header.crop((25,25,160,65)).tobytes()
  if clock_headers:assert title==clock_headers[0],case+' title shifts with clock width'
  clock_headers.append(title)
  assert next(r for r in results if r['case']==case and not r['sanitized'])['launches']==0
 # Actual raster frames must translate the same icon rows horizontally.
 # Exclude the clipped glyph region at the viewport edge: the existing
 # shared font renderer rounds negative fixed-point pen coordinates toward zero.
 if 'swipe-left' in (a.case or CASES):
  row=next(r for r in results if r['case']=='swipe-left' and not r['sanitized'])
  frames=[r for r in row['rendered'] if 780<r['ms']<960 and not r['highlight']]
  assert len(frames)>=2,'No real intermediate drag frames were rendered'
  for earlier,later in zip(frames,frames[1:]):
   delta=later['offset']-earlier['offset'];assert delta>0
   before=Image.open(out/f"swipe-left-0/frame-{earlier['frame']:02}.png")
   after=Image.open(out/f"swipe-left-0/frame-{later['frame']:02}.png")
   assert before.crop((delta+32,88,480,728)).tobytes()==after.crop((32,88,480-delta,728)).tobytes(),'Icons did not track the finger horizontally'
 # Fixed chrome never scrolls, even when a partially visible icon is clipped.
 for case in ('drag','bottom-tap','bounds','horizontal','stop-tap','busy-hit','new-contact','home',*[c for c in CASES if c.startswith('swipe-')]):
  directory=out/f'{case}-0'
  if case not in (a.case or CASES):continue
  images=[Image.open(path) for path in sorted(directory.glob('frame-*.png'))]
  if case in ('swipe-flipped','swipe-quick'):continue # Rotation and the overlay deliberately alter the chrome.
  for image in images[1:]:
   for box in ((0,0,480,88),(0,728,480,746),(0,786,480,800)):
    assert image.crop(box).tobytes()==images[0].crop(box).tobytes(),(case,box)
 if not a.case:
  normal=Image.open(out/'tap-0/endpoint.png');flipped=Image.open(out/'flipped-0/endpoint.png')
  assert normal.tobytes()==flipped.rotate(180).tobytes(),'global flip is not pixel exact'
  normal=Image.open(out/'swipe-left-0/endpoint.png');flipped=Image.open(out/'swipe-flipped-0/endpoint.png')
  assert normal.tobytes()==flipped.rotate(180).tobytes(),'swipe direction or final grid changed under global flip'
  for case in ('swipe-right','swipe-bounds','swipe-busy-reverse'):
   assert Image.open(out/f'{case}-0/endpoint.png').crop((0,728,480,800)).tobytes()==Image.open(out/'swipe-left-0/frame-01.png').crop((0,728,480,800)).tobytes(),case+' failed to present the latest page dots'
  assert Image.open(out/'transition-0/endpoint.png').tobytes()==Image.open(out/'drag-0/frame-01.png').tobytes(),'first-return crossfade changed endpoint'
 (out/'evidence.json').write_text(json.dumps({'hardware':'not run','cases':results},indent=2)+'\n')
if __name__=='__main__':main()
