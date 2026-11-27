#!/usr/bin/env python3
"""Actual Nova/controller + synthetic timing. Not measured hardware speed.

Charging counted renderer work excludes uncounted mask/arithmetic costs, so
this is a scheduling counterexample and regression, not a CPU benchmark.
"""
from pathlib import Path
import json

def main():
    import argparse,os,subprocess,hashlib,gzip
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args();r=Path(__file__).resolve().parents[1];o=args.output.resolve();o.mkdir(parents=True,exist_ok=True)
    a=(r/'lib/PortableApps/src/adapter.c').read_text()
    # Includes relative to adapter.c must continue resolving from original source directory.
    a=a.replace('static bool raster_checkpoint(void) {','extern void audit_work(unsigned);\nstatic bool raster_checkpoint(void) {\n audit_work(raster_pixels);',1)
    a=a.replace('static void clear_color(uint16_t color) {','extern void audit_frame_begin(void);\nstatic void clear_color(uint16_t color) {\n audit_frame_begin();',1)
    (o/'adapter-audit.c').write_text(a)
    f=(r/'test/native_apps/springboard_nova_test.c').read_text()
    f=f.replace('#include "../../Apps/SpringboardPresentation.h"',f'#include "{r}/Apps/SpringboardPresentation.h"')
    # Keep fixture API/capability scaffolding, replace only timed input/provider behavior and result expectations.
    f=f.replace('static unsigned present_started', 'static unsigned cost_ns, read_ms, bus_us, panel_ms, yield_extra_ms, last_work, gaps, max_queue, max_age, events_taken, cancellations, frame_start, max_frame, max_complete, completed_token, work_ns_remainder;\nstatic unsigned long long work_ns_total, poll_ms_total;\nstatic unsigned present_started',1)
    f=f.replace('static unsigned ms,polls,subs,grants,frames,presents,scenario,launches,last_present,max_presents;', 'static unsigned ms,polls,subs,grants,frames,presents,scenario,launches,last_present;\nvoid audit_frame_begin(void){frame_start=ms;}\nvoid audit_work(unsigned pixels){unsigned delta=pixels-last_work;last_work=pixels;unsigned long long ns=(unsigned long long)delta*cost_ns+work_ns_remainder;work_ns_total+=(unsigned long long)delta*cost_ns;unsigned dt=(unsigned)(ns/1000000);work_ns_remainder=(unsigned)(ns%1000000);ms+=dt;script_ms+=dt;}')
    f=f.replace('static unsigned logical_presents,last_logical_present,launch_ms;', 'static unsigned logical_presents,last_logical_present,launch_ms;')
    f=f.replace('if(logical_presents && scenario!=22)assert(ms-last_logical_present>=40);','/* Slow-cost experiment records cadence, rather than imposing an expected result. */')
    f=f.replace('static bool acquire_frame(void *c,uint32_t f,risc_display_surface_v1 *s){(void)c;assert(!frames);', 'static bool acquire_frame(void *c,uint32_t f,risc_display_surface_v1 *s){(void)c;assert(!frames);')
    f=f.replace('last_present=ms;*token=++presents;', 'if(ms-frame_start>max_frame){max_frame=ms-frame_start;}last_present=ms;*token=++presents;')
    start=f.index('static void script_snapshot(');end=f.index('static uint64_t subscribe',start)
    f=f[:start]+'''static void script_snapshot(unsigned step,risc_touch_snapshot_v1 *s){
     memset(s,0,sizeof(*s));s->width=TEST_WIDTH;s->height=TEST_HEIGHT;
     /* Independent 20ms physical sample updates; sustained triangular drag. */
     if(step>=5&&step<125){unsigned phase=(step-5)%40;unsigned x=phase<20?80+phase*4:160-(phase-20)*4;
     s->contact_count=1;s->contacts[0]=script_contact(0,x,120);}
    }
    '''+f[end:]
    f=f.replace('assert(touch_count<RISC_TOUCH_QUEUE_LENGTH);', 'if(touch_gap){return;} if(touch_count==RISC_TOUCH_QUEUE_LENGTH){gaps++;touch_head=touch_count=0;touch_gap=true;return;}')
    f=f.replace('.id=p.id,.x=p.x,.y=p.y};', '.id=p.id,.x=p.x,.y=p.y};\n if(touch_count>max_queue)max_queue=touch_count;')
    f=f.replace('(void)c;assert(n==1);++polls;', '(void)c;assert(n==1);++polls;ms+=read_ms;script_ms+=read_ms;poll_ms_total+=read_ms;')
    f=f.replace('*e=touch_events[touch_head];touch_head=', '*e=touch_events[touch_head];events_taken++;unsigned age=ms-(unsigned)e->timestamp_ms;if(age>max_age)max_age=age;touch_head=')
    f=f.replace('if(touch_gap){touch_gap=false;return -1;}', 'if(touch_gap){touch_gap=false;cancellations++;return -1;}')
    start=f.index('int main(')
    f=f[:start]+'''int main(int argc,char **argv){
     assert(argc==3);cost_ns=(unsigned)atoi(argv[1]);read_ms=(unsigned)atoi(argv[2]);scenario=99;
     assert(app_module_init()==0);app_main();app_module_fini();assert(!frames&&!grants&&!subs);
     printf("{\\"pixel_cost_ns\\":%u,\\"poll_cost_ms\\":%u,\\"elapsed_ms\\":%u,\\"render_pixel_ms\\":%.3f,\\"poll_ms\\":%llu,\\"polls\\":%u,\\"frames\\":%u,\\"max_completed_frame_ms\\":%u,\\"max_raw_poll_gap_ms\\":%u,\\"events_taken\\":%u,\\"max_dispatch_age_ms\\":%u,\\"max_queue\\":%u,\\"queue_gaps\\":%u,\\"gap_cancellations\\":%u,\\"launches\\":%u}\\n",cost_ns,read_ms,ms,work_ns_total/1000000.0,poll_ms_total,polls,presents,max_frame,max_touch_gap,events_taken,max_age,max_queue,gaps,cancellations,launches);
    }
    '''
    f=f.replace('scenario==24?33u:n;ms+=elapsed;', 'scenario==24?33u:n+yield_extra_ms;ms+=elapsed;')
    f=f.replace('ms+=read_ms;script_ms+=read_ms;poll_ms_total+=read_ms;', 'unsigned dt=read_ms+(bus_us*2+999)/1000;ms+=dt;script_ms+=dt;poll_ms_total+=dt;')
    f=f.replace('s->state=(scenario==36||scenario==37)&&presents>1&&ms-present_started<(scenario==36?120u:600u)?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;', 's->state=ms-present_started<panel_ms?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;')
    f=f.replace('assert(argc==3);cost_ns=', 'assert(argc==3||argc==6);if(argc==6){bus_us=(unsigned)atoi(argv[3]);panel_ms=(unsigned)atoi(argv[4]);yield_extra_ms=(unsigned)atoi(argv[5]);}cost_ns=')
    f=f.replace('return true;}\nstatic risc_touch_snapshot_v1 touch_state;', 'if(s->state==RISC_DISPLAY_PRESENT_COMPLETE&&t!=completed_token){completed_token=(unsigned)t;if(ms-frame_start>max_complete)max_complete=ms-frame_start;}return true;}\nstatic risc_touch_snapshot_v1 touch_state;')
    f=f.replace('max_frame,max_touch_gap,events_taken', 'max_complete,max_touch_gap,events_taken')
    f=f.replace('const char *dir=getenv("NOVA_FRAMES");if(!dir)return;', 'const char *dir=getenv("NOVA_FRAMES");if(!dir||presents!=1)return;')
    f=f.replace('static unsigned cost_ns,','static unsigned report_period=20,report_jitter,report_repeated,events_emitted;\nstatic unsigned cost_ns,',1)
    f=f.replace('return script_ms/20u;','if(!report_jitter)return script_ms/report_period;\n return (script_ms/(2*report_period))*2+((script_ms%(2*report_period))>=report_period-2);')
    f=f.replace('assert(argc==3||argc==6);','if(getenv("AUDIT_PERIOD_MS")){report_period=(unsigned)atoi(getenv("AUDIT_PERIOD_MS"));}assert(report_period>=5);report_jitter=getenv("AUDIT_JITTER")!=NULL;report_repeated=getenv("AUDIT_REPEATED")!=NULL;assert(argc==3||argc==6);')
    f=f.replace('if(touch_gap){return;} if(touch_count','events_emitted++;if(touch_gap){return;} if(touch_count',1)
    f=f.replace('if(step>=5&&step<125){unsigned phase=', 'if(step>=5&&step<125){if(report_repeated&&(step-5)%30>=24)return;unsigned phase=')
    f=f.replace('s->contacts[0]=script_contact(0,x,120);','s->contacts[0]=script_contact(report_repeated?((step-5)/30)%2:0,x,120);',1)
    f=f.replace('\\"events_taken\\":%u,','\\"events_taken\\":%u,\\"events_emitted\\":%u,').replace('events_taken,max_age','events_taken,events_emitted,max_age')
    f=f.replace('o->typical_present_latency_us=16000;', 'o->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;o->nominal_refresh_millihz=0;o->typical_present_latency_us=0;')
    f=f.replace('if(!presents)return 0;', 'if(!completed_token)return 0;').replace('completed_token=(unsigned)t;', 'if(!completed_token)script_ms=0;completed_token=(unsigned)t;')
    (o/'fixture.c').write_text(f)
    j=json.loads((r/'tests/fixtures/badge-native-latency-catalog.json').read_text())
    (o/'watch_suite_catalog.inc').write_text('\n'.join('{.display_name='+json.dumps(a['display_name'])+',.file_name='+json.dumps(a['file_name'])+',.icon='+json.dumps(a['icon'])+',.compatible=true},' for a in j['catalog']['apps'][:9])+'\n')
    base=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror']
    if args.sanitize:base+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all','-no-pie']
    base+=['-I'+str(o),'-I'+str(r/'lib/PortableApps/src'),'-I'+str(r/'lib/PortableApps/include'),'-I'+str(r/'lib/NativeApps/include')]
    common=['-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_NOVA_UI','-DPORTABLE_WATCH_NATIVE_HEIGHT','-DPORTABLE_WATCH_COLOR_ONLY','-DPORTABLE_FORCE_FULL_FRAMES','-DTEST_WIDTH=466','-DTEST_HEIGHT=466','-DNOVA_WATCH_CATALOG','-DCATALOG_COUNT=9']
    sliced=['-DPORTABLE_RASTER_WORK_BUDGET_MS=8','-DPORTABLE_RASTER_SNAPSHOT','-DPORTABLE_NOVA_SLICED_FRAMES','-DPORTABLE_FRAME_RECEIPTS','-DPORTABLE_INPUT_SAMPLE_INTERVAL_MS=2u','-DPORTABLE_RASTER_COOPERATIVE_INPUT','-DPORTABLE_RGB565_SPAN_FILL']
    records=[];commands=[];first_hashes={};first_pixels=None
    for name,extra in [('synchronous',[]),('sliced',sliced)]:
        exe=o/name;cmd=[*base,*common,*extra,str(r/'Apps/springboard.c'),str(o/'adapter-audit.c'),str(o/'fixture.c'),'-Wl,--wrap=t5_app_get_api','-o',str(exe)]
        commands.append(cmd);subprocess.run(cmd,check=True)
        for cost in [0,100,250,1000,2000]:
            raw=o/(name+'-first-'+str(cost));raw.mkdir(exist_ok=True)
            env=dict(os.environ,NOVA_FRAMES=str(raw));run=[str(exe),str(cost),'2','250','16','0']
            result=json.loads(subprocess.check_output(run,text=True,env=env,timeout=30));result.update(renderer=name,bus_us_per_transaction=250,panel_ms=16,yield_extra_ms=0)
            frames=list(raw.glob('*.rgb565'));assert len(frames)==1
            pixels=frames[0].read_bytes();digest=hashlib.sha256(pixels).hexdigest();first_hashes[name,cost]=digest
            if first_pixels is None:first_pixels=pixels
            else:assert pixels==first_pixels
            frames[0].with_suffix('.rgb565.gz').write_bytes(gzip.compress(pixels));frames[0].unlink()
            result['first_frame_sha256']=digest;records.append(result)
            assert result['launches']==0
            if name=='sliced':assert result['events_taken']==121 and result['queue_gaps']==0 and result['max_dispatch_age_ms']<=20
        if name=='synchronous':assert any(x['queue_gaps'] for x in records if x['renderer']==name)
    assert len(set(first_hashes.values()))==1,first_hashes
    sweeps=[]
    for period in [20,10,5]:
        for jitter in [False,True]:
            for repeated in [False,True]:
                env=dict(os.environ,AUDIT_PERIOD_MS=str(period))
                if jitter:env['AUDIT_JITTER']='1'
                if repeated:env['AUDIT_REPEATED']='1'
                run=[str(o/'sliced'),'250','2','250','16','1']
                result=json.loads(subprocess.check_output(run,env=env,text=True,timeout=30))
                result.update(report_period_ms=period,jitter=jitter,repeated=repeated)
                assert result['events_taken']==result['events_emitted'] and result['queue_gaps']==0 and result['launches']==0
                assert result['max_dispatch_age_ms']<=40
                sweeps.append(result)
    # Same adapter with a terminal display callback; the fixture observes every
    # later provider entry rather than assuming cleanup remained fenced.
    import re
    fault=f.replace('static unsigned cost_ns,', 'static bool retained_seen;static unsigned after_retained;\nstatic void provider_entry(void){if(retained_seen)after_retained++;}\nstatic unsigned cost_ns,',1)
    fault=fault.replace('assert(!"unexpected retained fixture invocation");return true;', 'retained_seen=true;return true;')
    fault=fault.replace('static bool present_status(void *c,risc_display_present_token_v1 t,risc_display_present_status_v1 *s){', 'static bool present_status(void *c,risc_display_present_token_v1 t,risc_display_present_status_v1 *s){if(presents>=2)return false;',1)
    for name in ['health','yield_ms','diagnostic','launch_app','get_info','acquire_frame','release_frame','submit','present_status','subscribe','unsubscribe','poll_touch','next_touch','snapshot','battery_read','catalog_snapshot','catalog_get','exit_requested','acquire','release']:
        pattern=r'(static [^{};]+\b'+name+r'\([^{};]*\)\s*\{)';fault,n=re.subn(pattern,r'\1provider_entry();',fault,count=1);assert n==1,name
    fault=fault.replace('assert(!frames&&!grants&&!subs);','assert(retained_seen&&!after_retained);')
    (o/'fault-fixture.c').write_text(fault)
    cmd=[*base,*common,*sliced,str(r/'Apps/springboard.c'),str(o/'adapter-audit.c'),str(o/'fault-fixture.c'),'-Wl,--wrap=t5_app_get_api','-o',str(o/'fault')];commands.append(cmd);subprocess.run(cmd,check=True)
    fault_result=json.loads(subprocess.check_output([str(o/'fault'),'100','2','250','16','0'],text=True,timeout=30))
    # Known allocation refusal preserves the old synchronous compatibility
    # fallback. It is usable, but deliberately not claimed latency-bounded.
    alloc=f.replace('static unsigned cost_ns,','static unsigned alloc_refused;\nstatic unsigned cost_ns,',1)
    alloc=alloc.replace('assert(!frames&&!grants&&!subs);','assert(!frames&&!grants&&!subs&&alloc_refused==1);')
    alloc+='\nvoid *__real_malloc(size_t);\nvoid *__wrap_malloc(size_t n){const char *mode=getenv("AUDIT_ALLOC");if(mode&&!alloc_refused&&(!strcmp(mode,"command")||n>=466u*466u*2u)){alloc_refused++;return NULL;}return __real_malloc(n); }\n'
    (o/'alloc-fixture.c').write_text(alloc)
    cmd=[*base,*common,*sliced,str(r/'Apps/springboard.c'),str(o/'adapter-audit.c'),str(o/'alloc-fixture.c'),'-Wl,--wrap=t5_app_get_api','-Wl,--wrap=malloc','-o',str(o/'alloc')];commands.append(cmd);subprocess.run(cmd,check=True)
    alloc_results=[]
    for mode in ['command','pixels']:
        raw=o/('alloc-'+mode);raw.mkdir(exist_ok=True)
        env=dict(os.environ,AUDIT_ALLOC=mode,NOVA_FRAMES=str(raw))
        result=json.loads(subprocess.check_output([str(o/'alloc'),'0','2','250','16','0'],env=env,text=True,timeout=30));result['allocation_refusal']=mode
        frames=list(raw.glob('*.rgb565'));assert len(frames)==1
        pixels=frames[0].read_bytes();assert pixels==first_pixels
        frames[0].with_suffix('.rgb565.gz').write_bytes(gzip.compress(pixels));frames[0].unlink()
        assert result['launches']==0;alloc_results.append(result)
    ordinary_source=(r/'test/native_apps/springboard_nova_test.c').read_text().replace('#include "../../Apps/SpringboardPresentation.h"', '#include "'+str(r/'Apps/SpringboardPresentation.h')+'"').replace('o->typical_present_latency_us=16000;', 'o->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;o->typical_present_latency_us=16000;')
    (o/'ordinary-fixture.c').write_text(ordinary_source)
    ordinary=o/'ordinary';cmd=[*base,*common,*sliced,str(r/'Apps/springboard.c'),str(r/'lib/PortableApps/src/adapter.c'),str(o/'ordinary-fixture.c'),'-Wl,--wrap=t5_app_get_api','-o',str(ordinary)];commands.append(cmd);subprocess.run(cmd,check=True)
    ordinary_cases=[0,1,2,6,14,15,16,23,24,36,37,3,4,5,17,18,25,26,27,28,38]
    ordinary_log=''.join(subprocess.check_output([str(ordinary),str(case)],text=True,timeout=30) for case in ordinary_cases)
    (o/'ordinary.log').write_text(ordinary_log)
    map_checks=[]
    for name in ['springboard_completed_map_test','frame_receipt_test']:
        exe=o/name;cmd=[*base,str(r/('test/native_apps/'+name+'.c')),'-o',str(exe)];commands.append(cmd);subprocess.run(cmd,check=True)
        map_checks.append(subprocess.check_output([str(exe)],text=True,timeout=30).strip())
    receipt={'display_async_present':True,'selected_work_budget_ms':8,'historical_default_work_budget_ms':2,'kind':'synthetic-timing-regression-not-device-benchmark','sanitized':args.sanitize,'commands':commands,'scenarios':records,'rate_jitter_sweep':sweeps,'retained_display_callback':fault_result,'allocation_fallback':alloc_results,'ordinary_cases':ordinary_cases,'frame_map_checks':map_checks,'asan_options':os.environ.get('ASAN_OPTIONS','')}
    (o/'results.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('Native sliced scheduling: 25 timing/fault/fallback cases, 21 ordinary cases, two frame-map checks; exact first-frame pixels PASS')

if __name__=='__main__':main()
