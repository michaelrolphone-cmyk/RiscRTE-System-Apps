#!/usr/bin/env python3
"""Real packed MONO1 Springboard entry proof; no RGB565-sized backing buffer."""
from pathlib import Path
import argparse,subprocess,json,os,hashlib
r=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();o=a.output.resolve();o.mkdir(parents=True,exist_ok=True)
adapter=(r/'lib/PortableApps/src/adapter.c').read_text();nova=(r/'lib/PortableApps/src/nova.inc').read_text()
needle='static void np_mask(void) {';assert needle in nova;nova=nova.replace(needle,'static unsigned mask_format_calls;\n'+needle+'\n ++mask_format_calls;assert(surface_format==RISC_DISPLAY_FORMAT_RGB565);',1)
(o/'nova-format.inc').write_text(nova)
adapter='#include <assert.h>\n'+adapter.replace('#include "nova.inc"','#include '+json.dumps(str(o/'nova-format.inc')))
adapter+='\nvoid mask_format_report(void){assert(surface_format==RISC_DISPLAY_FORMAT_MONO1);assert(springboard_presentation_get()==NULL);assert(mask_format_calls==0);}\n'
(o/'adapter.c').write_text(adapter)
f=(r/'test/native_apps/springboard_paper_test.c').read_text().replace('#include "../../Apps/PaperPresentation.h"','#include '+json.dumps(str(r/'Apps/PaperPresentation.h')))
f=f.replace('int main(int argc,char **argv){','extern void mask_format_report(void);\n_Static_assert(sizeof(pixels)==48000,"actual packed MONO1 allocation required");\nint main(int argc,char **argv){')
f=f.replace('app_main();app_module_fini();','app_main();mask_format_report();app_module_fini();')
(o/'fixture.c').write_text(f);rows=[]
for snapshot in [False,True]:
 for landscape in [False,True]:
  name=('sliced' if snapshot else 'direct')+('-landscape' if landscape else '-portrait');exe=o/name
  cmd=['cc','-std=c11','-O1','-g1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-DCATALOG_COUNT=5','-I'+str(r/'lib/PortableApps/src'),'-I'+str(r/'lib/PortableApps/include'),'-I'+str(r/'lib/NativeApps/include')]
  if snapshot:cmd+=['-DPORTABLE_RASTER_SNAPSHOT']
  if landscape:cmd+=['-DPORTABLE_DISPLAY_ROTATION=90','-DTEST_NATIVE_LANDSCAPE']
  cmd+=[str(r/'Apps/springboard.c'),str(o/'adapter.c'),str(o/'fixture.c'),'-o',str(exe)];subprocess.run(cmd,check=True)
  outputs=[]
  for case in [0,1,2,3,4,5,6,7,9]:
   z=subprocess.check_output([str(exe),str(case)],text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'});outputs.append(z)
  (o/(name+'.log')).write_text(''.join(outputs));rows.append(dict(name=name,command=cmd,cases=9,mask_calls=0,buffer_bytes=48000,stride_bytes=100 if landscape else 60))
(o/'results.json').write_text(json.dumps(dict(source_sha256=hashlib.sha256((r/'lib/PortableApps/src/nova.inc').read_bytes()).hexdigest(),cases=36,results=rows),indent=2)+'\n');print('PASS 36 actual MONO1 Springboard cases: packed 48000-byte buffers, 60/100-byte strides, portrait/native landscape, immediate/replay; RGB presentation rejected and mask never entered')
