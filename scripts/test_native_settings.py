#!/usr/bin/env python3
"""Capability-sized Settings against the production controller and raw providers.

The retained fixture is staged with dimensioned provider storage only. Native
interaction expectations are independent and never call the legacy coordinate
translation. All generated files remain under the requested output directory.
"""
import argparse, hashlib, json, os, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SIZES=((240,240),(410,502),(128,160),(160,128),(502,240))
def stage_fixture(out, name):
    src=(ROOT/'test/native_apps'/name).read_text()
    src=src.replace('#include "../../lib/PortableApps/src/adapter.c"',f'#include "{ROOT}/lib/PortableApps/src/adapter.c"')
    src=src.replace('#include "nova_settings_coordinates.h"','')
    src=src.replace('static uint16_t framebuffer[240*240];','static uint16_t *framebuffer;')
    src=src.replace('static uint16_t framebuffer[240 * 240];','static uint16_t *framebuffer;')
    src=src.replace('.width=240,.height=240','.width=test_width,.height=test_height')
    src=src.replace('.stride_bytes=480,.size_bytes=sizeof(framebuffer)', '.stride_bytes=test_stride,.size_bytes=test_size')
    src=src.replace('out->width=out->height=240;', 'out->width=test_width;out->height=test_height;')
    src=src.replace('    nova_fixture_choice_coordinates(sv_page,&out->contacts[0].x,&out->contacts[0].y);','')
    src=src.replace('239-out->contacts[0].x','test_width-1-out->contacts[0].x').replace('239-out->contacts[0].y','test_height-1-out->contacts[0].y')
    dst=out/name;dst.write_text(src);return dst

def stage_timezone(out):
    base=stage_fixture(out,'portable_settings_test.c')
    s=(ROOT/'test/native_apps/portable_timezone_settings_test.c').read_text()
    s=s.replace('#include "portable_settings_test.c"',f'#include "{base}"')
    s=s.replace('int main(int argc,char **argv)', 'static int retained_timezone_main(int argc,char **argv)')
    s=s.replace('assert(count>0&&count<=2)', 'assert(count>0&&count<=3)')
    s=s.replace('pp_enabled()?stz_height()-164:190','(int)test_height-8-(test_height<200?28:44)-(test_width<160?32:0)-(test_height<200?24:32)/2-2')
    start=s.index('static void touch_footer(');end=s.index('static unsigned city_number',start)
    s=s[:start]+'''static void touch_footer(unsigned slot,bool cities) {
      int w=(int)test_width,h=(int)test_height,margin=w<200?8:20,gap=w<200?4:6,fh=h<200?28:44,y=h-8-fh;
      if(w<160) {
        if(cities&&slot==0){touch_at(w/2,y-32+14);return;}
        unsigned index=slot-(cities?1:0);int half=(w-2*margin-gap)/2;
        touch_at(margin+(int)index*(half+gap)+half/2,y+fh/2);return;
      }
      int columns=cities?3:2,fw=(w-2*margin-(columns-1)*gap)/columns;
      touch_at(margin+(int)slot*(fw+gap)+fw/2,y+fh/2);
    }
    ''' +s[end:]
    s=s.replace('touch_at(pp_enabled()?120:50,pp_enabled()?SP_TOP+SP_ROW+30:sv_row_y(1)+10);', 'settings_render(0,2);touch_at(test_width/2,sn_geometry().top+(sn_visible_rows()>1?sn_geometry().row:0)+10);')
    s=s.replace('contact_at(at,60,stz_top()+10)', 'contact_at(at,test_width/4,stz_top()+10)').replace('contact_at(at+1,140,stz_top()+10)', 'contact_at(at+1,test_width*3/4,stz_top()+10)')
    # Native row text has up to three bounded lines, not the legacy two-line assumption.
    s=s.replace('setup();text_checks();','''setup();text_checks();
      assert(sn_enabled()&&stz_native_fits());
      assert(stz_top()+(int)stz_rows()*stz_step()<=stz_page_y()-2);
      for(unsigned i=0;i<3;i++){
        sn_rect r=stz_footer_rect(i,true);assert(r.x>=0&&r.y>=0&&r.x+r.w<=width()&&r.y+r.h<=height());
        assert(r.w>=sv_measure(2,i==0?"BACK":i==1?"CANCEL":"SAVE",0)+6);
      }''')
    s=s.replace('fprintf(f,"P6\\n240 240\\n255\\n");','fprintf(f,"P6\\n%u %u\\n255\\n",test_width,test_height);')
    s=s.replace('for(unsigned i=0;i<240*240;++i){unsigned v=framebuffer[i];', 'for(unsigned y=0;y<test_height;y++)for(unsigned x=0;x<test_width;x++){const uint8_t*p=(const uint8_t*)framebuffer+y*test_stride+x*2;unsigned v=p[0]+256u*p[1];')
    s=s.replace('finish();printf("Timezone Settings mode %u passed\\n",mode);', 'finish();printf("Timezone Settings mode %u passed\\n",mode);return 0;')
    fixture=out/'timezone_fixture.c';fixture.write_text(s);return fixture

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=ROOT/'build/native-settings');parser.add_argument('--timezone-only',action='store_true')
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    fixture=stage_fixture(out,'portable_alarm_settings_test.c');runs=[]
    env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'}
    for san in (() if args.timezone_only else (False,True)):
        for rotation in (0,180):
            name=f'native-settings-{rotation}-{int(san)}';binary=out/name;frames=out/'frames';frames.mkdir(exist_ok=True)
            cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function',
                 '-DPORTABLE_SETTINGS_NATIVE_UI','-DPORTABLE_NATIVE_CANVAS','-DPORTABLE_NOVA_UI','-DPORTABLE_SLEEP_SETTINGS',
                 '-DPORTABLE_TOUCH_ROTATION='+str(rotation),f'-DSETTINGS_FRAME_FIXTURE="{fixture}"',
                 '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include')]
            if san:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
            cmd += [str(ROOT/'Apps/settings.c'),str(ROOT/'test/native_apps/native_settings_test.c'),'-o',str(binary)]
            subprocess.run(cmd,check=True,timeout=120)
            for w,h in SIZES:
                result=subprocess.run([str(binary),str(frames),str(w),str(h)],env=env,check=True,capture_output=True,text=True,timeout=30)
                print(result.stdout,end='');runs.append({'size':[w,h],'rotation':rotation,'sanitizers':san,'result':'passed'})
            binary.unlink()
    zone=stage_timezone(out)
    for san in (False,True):
        for rotation in (0,180):
            binary=out/f'native-timezone-{rotation}-{int(san)}'
            cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function',
                '-DPORTABLE_SETTINGS_NATIVE_UI','-DPORTABLE_NATIVE_CANVAS','-DPORTABLE_NOVA_UI','-DTEST_TIMEZONE_COMPACT',
                '-DPORTABLE_TOUCH_ROTATION='+str(rotation),f'-DSETTINGS_TIMEZONE_FIXTURE="{zone}"',
                '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include')]
            if san:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
            cmd += [str(ROOT/'Apps/settings.c'),str(ROOT/'test/native_apps/native_timezone_settings_test.c')]
            cmd += [str(ROOT/'lib/PortableApps/src'/name) for name in ['PortableTimeZone.c','PortableTimeZoneCatalog.c','PortableTimeZonePreference.c']]
            subprocess.run([*cmd,'-o',str(binary)],check=True,timeout=120)
            for w,h in SIZES:
                cases=[(m,i) for m in (0,1) for i in (0,100,418)]+[(m,) for m in range(2,30)]+[(36,)]
                for case in cases:
                    result=subprocess.run([str(binary),str(w),str(h),*map(str,case)],env=env,capture_output=True,text=True,timeout=30)
                    if result.returncode:raise RuntimeError(f'timezone {w}x{h} rotation={rotation} san={san} case={case}: '+result.stdout+result.stderr)
                    runs.append({'suite':'timezone','size':[w,h],'rotation':rotation,'sanitizers':san,'case':case,'result':'passed'})
                print(f'Native timezone {w}x{h} rotation={rotation} san={san}: {len(cases)} real-controller cases passed',flush=True)
            binary.unlink()
    hashes={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [ROOT/'test/native_apps/native_settings_test.c',*sorted((ROOT/'lib/PortableApps/src').glob('settings*.inc'))]}
    (out/'receipt.json').write_text(json.dumps({'source_base':'fd9a028c5d38eb517c1328b869684c4330a5d4d9','runs':runs,'source_sha256':hashes,'hardware':'not-tested'},indent=2)+'\n')
    print(f'Native Settings: {len(runs)} actual-controller runs passed')
if __name__=='__main__':main()
