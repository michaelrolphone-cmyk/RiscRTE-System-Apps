#!/usr/bin/env python3
"""Actual selected UC8279 brightness during ACTIVE; IO-only model, no hardware."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--product',type=Path,required=True);p.add_argument('--sdk',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
a.product=a.product.resolve();a.sdk=a.sdk.resolve();a.output.mkdir(parents=True,exist_ok=True)
f=a.product/'minimal/test/uc8279_fast_test.c';driver=a.product/'minimal/drivers/x4pro_uc8279_fast/driver.c';s=f.read_text()
old='  ((const risc_driver_poll_v2*)d)->poll(8);assert(present_state==PRESENT_ACTIVE);';assert s.count(old)==1
s=s.replace(old,old+"\n  { const tone_display_state before_brightness=tone_display_snapshot();\n    assert(output->set_brightness(NULL,63,100));\n    assert(light_level==63 && light_maximum==100);\n    const tone_display_state after_brightness=tone_display_snapshot();\n    assert(!memcmp(&before_brightness,&after_brightness,sizeof(before_brightness))); }")
s=s.replace('#include "../drivers/x4pro_uc8279_fast/driver.c"','#include "'+str(driver)+'"');source=a.output/'fixture.c';source.write_text(s)
for sanitized in (False,True):
 exe=a.output/('san' if sanitized else 'normal');flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
 cmd=[os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-pedantic',*flags,'-I'+str(f.parent),'-I'+str(a.sdk),str(source),'-o',str(exe)]
 subprocess.run(cmd,check=True);r=subprocess.run([str(exe),'tone-forward'],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},capture_output=True,text=True);(a.output/(exe.name+'.log')).write_text(r.stdout+r.stderr)
receipt={'hardware':False,'source_sha256':{str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in (f,driver,source)},'cases':['normal','ASan/UBSan'],'display_state_unchanged':True,'brightness_during_PRESENT_ACTIVE':63}
(a.output/'evidence.json').write_text(json.dumps(receipt,indent=2)+'\n');print('ACTIVE brightness preserves complete display state: normal and ASan/UBSan PASS')
