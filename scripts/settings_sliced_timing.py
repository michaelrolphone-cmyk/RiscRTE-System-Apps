"""Actual Settings adapter/render timing model; no hardware claims."""
from pathlib import Path
import subprocess,json,os
def run(base,out,sanitize=False):
 src=out
 for lane in ['sync','sliced']:
  d=out/lane;d.mkdir(exist_ok=True)
  a=(src/'adapter.c').read_text().replace('static bool raster_checkpoint(void) {','extern void audit_work(unsigned);\nstatic bool raster_checkpoint(void) {audit_work(raster_pixels);',1)
  a=a.replace('input_progressed=portable_touch_next(&touch,&next);','input_progressed=portable_touch_next(&touch,&next);\n extern void audit_dispatch(bool,unsigned);audit_dispatch(input_progressed,(unsigned)next.timestamp_ms);')
  (d/'adapter.c').write_text(a)
  view=(src/'settings_view.inc').read_text().replace('static void sv_begin(void) { clear();','extern void audit_frame_begin(void);\nstatic void sv_begin(void) { audit_frame_begin();clear();')
  (d/'settings_view.inc').write_text(view)
  a=a.replace(str(src/'settings.inc'),str(d/'settings.inc'));(d/'adapter.c').write_text(a)
  (d/'settings.inc').write_text((src/'settings.inc').read_text().replace(str(src/'settings_view.inc'),str(d/'settings_view.inc')))
  p=(src/'portable_settings_test.c').read_text().replace(str(src/'adapter.c'),str(d/'adapter.c'))
  p=p.replace('static unsigned scenario, ticks, polls, grants, subscriptions, frame_count, displays;', '''static unsigned scenario, ticks, polls, grants, subscriptions, frame_count, displays;
 static unsigned cost_ns,last_pixels,report_period=10,jitter,poll_us=2250,panel_ms=16,submit_at,frame_start,max_frame,max_dispatch,dispatched,emitted,gaps,completed,completed_token,audit_start,birth;static bool audit_active,touch_gap;static unsigned long long ns,work_ns,poll_ns;
 static void advance(unsigned long long n){ns+=n;ticks=(unsigned)(ns/1000000);}
 void audit_work(unsigned n){unsigned delta=n>=last_pixels?n-last_pixels:n;last_pixels=n;work_ns+=(unsigned long long)delta*cost_ns;advance((unsigned long long)delta*cost_ns);}
 void audit_frame_begin(void){frame_start=ticks;}
 void audit_dispatch(bool progressed,unsigned timestamp){if(audit_active&&progressed&&timestamp){unsigned age=ticks-timestamp;if(age>max_dispatch)max_dispatch=age;dispatched++;}}
 ''')
  p=p.replace('#ifdef TEST_SETTINGS_ASYNC\n  out->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;\n#endif','out->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;')
  p=p.replace('ticks += ms;','advance((unsigned long long)ms*1000000);')
  p=p.replace('frame_count=0;*token=++displays;','frame_count=0;*token=++displays;submit_at=ticks;')
  p=p.replace('out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;', '''if(ticks-submit_at<panel_ms){out->state=RISC_DISPLAY_PRESENT_ACTIVE;return true;}
 if(completed_token!=token){completed_token=(unsigned)token;completed++;if(ticks-frame_start>max_frame)max_frame=ticks-frame_start;}
 out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;''')
  p=p.replace('/* Held-entry case starts before subscription;', '''if(audit_active){
  unsigned t=ticks-audit_start;unsigned step=t/report_period;
  if(jitter){unsigned pair=t/(2*report_period),rem=t%(2*report_period);step=pair*2+(rem>=report_period-2);}
  birth=audit_start+step*report_period;
  if(jitter&&(step&1))birth-=2;
  if(t>=30&&t<630){unsigned phase=step%40;int y=phase<20?130+(int)phase*8:290-(int)(phase-20)*8;out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=233,.y=(uint16_t)y};}
  return true;}
 /* Held-entry case starts before subscription;''')
  p=p.replace('assert(touch_count<RISC_TOUCH_QUEUE_LENGTH);','if(touch_count==RISC_TOUCH_QUEUE_LENGTH){touch_gap=true;touch_count=touch_head=0;gaps++;return;}emitted++;')
  p=p.replace('.timestamp_ms=ticks,.kind=kind','.timestamp_ms=audit_active?birth:ticks,.kind=kind')
  p=p.replace('(void)c;assert(n==1);++polls;','(void)c;assert(n==1);++polls;advance((unsigned long long)poll_us*1000);poll_ns+=(unsigned long long)poll_us*1000;')
  p=p.replace('if(!touch_count)return 0;','if(touch_gap){touch_gap=false;touch_count=touch_head=0;return -1;}if(!touch_count)return 0;')
  (d/'portable_settings_test.c').write_text(p)
  (d/'settings_sleep_defaults_test.c').write_text((src/'settings_sleep_defaults_test.c').read_text().replace(str(src/'portable_settings_test.c'),str(d/'portable_settings_test.c')))
  (d/'main.c').write_text('''#define PORTABLE_SETTINGS_BOTH_TIMERS
 #define main inherited_main
 #include "settings_sleep_defaults_test.c"
 #undef main
 int main(int argc,char**argv){assert(argc==4);cost_ns=atoi(argv[1]);report_period=atoi(argv[2]);jitter=atoi(argv[3]);begin();
 for(unsigned i=0;i<100000&&!portable_paper_frame_idle();i++){assert(paper_present_progress());test_yield(1);}
 assert(portable_paper_frame_idle());
 #ifdef PORTABLE_SETTINGS_SLICED_FRAMES
 sv_frame_completed();assert(sv_frame_current());
 #endif
 audit_active=true;audit_start=ticks;max_dispatch=dispatched=emitted=gaps=max_frame=completed=0;work_ns=poll_ns=0;
 /* Same initial root content, followed by actual controller polling/drag redraws. */
 settings_view_redraw();for(unsigned i=0;i<100000&&ticks-audit_start<1500;i++){t5_app_input_t input={0};assert(poll(&input,20));}
 printf("{\\"cost_ns\\":%u,\\"period_ms\\":%u,\\"jitter\\":%u,\\"polls\\":%u,\\"emitted\\":%u,\\"dispatched\\":%u,\\"gaps\\":%u,\\"max_dispatch_ms\\":%u,\\"max_frame_ms\\":%u,\\"completed\\":%u,\\"raster_ms\\":%.3f,\\"poll_ms\\":%.3f}\\n",cost_ns,report_period,jitter,polls,emitted,dispatched,gaps,max_dispatch,max_frame,completed,work_ns/1e6,poll_ns/1e6);app_module_fini();assert(!grants&&!subscriptions&&!frame_count);}
 ''')
  flags=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-DPORTABLE_SETTINGS_NATIVE_GEOMETRY','-DPORTABLE_SAFE_VIEWPORT','-DTEST_SETTINGS_RASTER_WIDTH=466','-DTEST_SETTINGS_RASTER_HEIGHT=466','-DTEST_SETTINGS_INSET=69','-I'+str(base/'lib/PortableApps/include'),'-I'+str(base/'lib/NativeApps/include')]
  if lane=='sliced':flags+=['-DPORTABLE_SETTINGS_SLICED_FRAMES','-DPORTABLE_RASTER_SNAPSHOT','-DPORTABLE_FRAME_RECEIPTS','-DPORTABLE_RASTER_WORK_BUDGET_MS=8','-DPORTABLE_RASTER_COOPERATIVE_INPUT','-DPORTABLE_INPUT_SAMPLE_INTERVAL_MS=2u']
  if sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
  cmd=flags+[str(base/'Apps/settings.c'),str(d/'main.c'),'-o',str(d/'test')];(d/'command.json').write_text(json.dumps(cmd));subprocess.run(cmd,check=True)
  rows=[]
  for cost in [100,1000]:
   for period in [5,10]:
    for jitter in [0,1]:
     p=subprocess.run([str(d/'test'),str(cost),str(period),str(jitter)],text=True,capture_output=True);(d/f'{cost}-{period}-{jitter}.log').write_text(p.stdout+p.stderr)
     if p.returncode:raise RuntimeError((lane,cost,period,jitter,p.stderr))
     rows.append(json.loads(p.stdout))
  (d/'results.json').write_text(json.dumps(rows,indent=2))
  print(lane,rows)
  (d/'test').unlink()
