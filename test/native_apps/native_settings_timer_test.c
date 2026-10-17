/* Actual Settings timer controller with dimensioned providers and independent
 * native-coordinate expectations. Production sources are never transformed. */
#include <stdint.h>
static unsigned test_width,test_height,test_stride,test_size,native_script_started;
#define TEST_SETTINGS_CUSTOM_RUNTIME
#define PORTABLE_QUICK_FIXTURE_MAIN retained_quick_main
#include SETTINGS_TIMER_FIXTURE
static uint8_t *guarded_frame;
static bool runtime_exit;
static unsigned test_case;
static const char *frame_directory;
static bool native_health(risc_runtime_health_v1*out){active_provider();assert(ticks<20000);out->uptime_ms=ticks;return true;}
static bool native_exit(void){return runtime_exit;}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){return version==1?&quick_runtime:NULL;}
static void guard_check(void){
 const uint8_t*p=(const uint8_t*)framebuffer;
 for(unsigned i=0;i<32;i++)assert(guarded_frame[i]==0xa5&&guarded_frame[32+test_size+i]==0xa5);
 for(unsigned y=0;y<test_height;y++)for(unsigned x=test_width*2;x<test_stride;x++)assert(p[y*test_stride+x]==0xa5);
}
static unsigned pixel(unsigned x,unsigned y){const uint8_t*p=(const uint8_t*)framebuffer+y*test_stride+2*x;return p[0]+256u*p[1];}
static unsigned nonblack(unsigned x,unsigned y,unsigned w,unsigned h){unsigned count=0;for(unsigned j=y;j<y+h;j++)for(unsigned i=x;i<x+w;i++)count+=pixel(i,j)!=0;return count;}
static void capture(const char*name){
 if(!frame_directory)return;
 char path[1024];snprintf(path,sizeof(path),"%s/%ux%u-%s.ppm",frame_directory,test_width,test_height,name);
 FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",test_width,test_height);
 for(unsigned y=0;y<test_height;y++)for(unsigned x=0;x<test_width;x++){
  unsigned v=pixel(x,y);unsigned char rgb[]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};assert(fwrite(rgb,1,3,f)==3);
 }assert(!fclose(f));guard_check();
}
static void native_setup(void){
 scenario=100;alarm_scenario=0;input_count=polls=0;
 quick_display_api=display_api;quick_display_api.set_brightness=quick_brightness;
 quick_alarm_api=service_api;quick_touch_api=touch_api;
 quick_runtime=runtime_api;quick_runtime.acquire=quick_acquire;quick_runtime.health=native_health;quick_runtime.request_launch=quick_launch;quick_runtime.exit_requested=native_exit;
 alarm_fake.state=ALARM_STATE_READY;assert(app_module_init()==0);assert(sn_enabled());
 assert(pqa_session_load(&quick,rt));settings_render(0,0);assert(portable_paper_frame_drain());assert(!failed);guard_check();
}
static void script_reset(void){input_count=0;native_script_started=ticks;}
static unsigned margin(void){return test_width<200?8:20;}
static unsigned top(void){return test_height<200?28:56;}
static unsigned side(void){return test_width<200?32:44;}
static unsigned footer_h(void){return test_height<200?28:44;}
static unsigned footer_y(void){return test_height-8-footer_h();}
static unsigned half(void){return (test_width-2*margin()-(test_width<200?6:12))/2;}
static void adjustment(unsigned at,int direction){tap(at,direction>0?test_width-margin()-side()/2:margin()+side()/2,top()+side()/2);}
static void footer(unsigned at,bool cancel){tap(at,cancel?margin()+half()/2:test_width-margin()-half()/2,footer_y()+footer_h()/2);}
static void seed(bool deep,unsigned value){assert(portable_sleep_timer_save(settings_store,deep,value));assert(pqa_session_load(&quick,rt));}
static void loaded_is(bool deep,unsigned value){uint32_t got=0;assert(portable_sleep_timer_load(settings_store,deep,&got)==PORTABLE_SLEEP_LOADED&&got==value);assert((deep?quick.deep_ms:quick.idle_ms)==value);}
static uint8_t choose(bool deep,int direction,bool cancel){script_reset();if(direction)adjustment(10,direction);footer(20,cancel);footer(40,true);uint8_t result=settings_activate(0,SETTINGS_TIMER_ROW+(unsigned)deep);guard_check();return result;}
static void geometry(void){
 unsigned m=margin(),s=side(),t=top(),fy=footer_y(),fh=footer_h(),hw=half(),gap=test_width<200?6:12;
 /* Expectations here derive only from the native layout contract. No sn_rect,
  * sn_control, sn_footer, or legacy coordinate converter supplies test points. */
 const int identities[4][4]={{20,88,64,44},{156,88,64,44},{24,200,86,26},{130,200,86,26}};
 const unsigned boxes[4][4]={{m,t,s,s},{test_width-m-s,t,s,s},{m,fy,hw,fh},{m+hw+gap,fy,hw,fh}};
 for(unsigned n=0;n<4;n++){
  unsigned x=boxes[n][0],y=boxes[n][1],w=boxes[n][2],h=boxes[n][3];const int*i=identities[n];
  assert(sv_inside(x,y,i[0],i[1],i[2],i[3]));assert(sv_inside(x+w-1,y+h-1,i[0],i[1],i[2],i[3]));
  assert(!sv_inside(x+w,y,i[0],i[1],i[2],i[3]));assert(!sv_inside(x,y+h,i[0],i[1],i[2],i[3]));
  assert(nonblack(x,y,w,h)>20);assert(pixel(x+w/2,y)!=0&&pixel(x,y+h/2)!=0);
 }
 /* Both timer lines must fit the central gap without touching +/- controls. */
 assert((unsigned)portable_nova_measure(2,"3600")<=test_width-2*(m+s));
 assert((unsigned)portable_nova_measure(2,"SECONDS")<=test_width-2*(m+s));
 assert(nonblack(m+s,t,test_width-2*(m+s),16)>0);
 assert(nonblack(m+s,t+16,test_width-2*(m+s),16)>0);
 assert(t+32<fy-15);guard_check();
}
static void direct_sample(portable_touch_sample sample,t5_app_input_t*out){
 input_progressed=true;input_pending=true;input_sample=sample;memset(out,0,sizeof(*out));assert(settings_view_poll(out));
}
static t5_app_input_t raw_at(unsigned slot){
 ticks=native_script_started+slot*20u;assert(portable_touch_collect(&touch));t5_app_input_t emitted={0};
 for(unsigned n=0;n<16;n++){
  portable_touch_sample sample;bool progressed=portable_touch_next(&touch,&sample);t5_app_input_t out;
  direct_sample(sample,&out);if(out.tapped||out.buttons||out.exit_requested)emitted=out;
  if(!progressed)return emitted;
 }assert(!"raw stream did not drain");return emitted;
}
static void gate_test(void){
 timer_deep=false;timer_choice=60000;sv_switch(SV_TIMER);assert(portable_paper_frame_drain());
 unsigned x=test_width-margin()-side()/2,y=top()+side()/2;t5_app_input_t out;
 for(unsigned state=0;state<
#ifdef PORTABLE_RASTER_SNAPSHOT
 5
#else
 3
#endif
 ;state++){
  script_reset();tap(10,x,y);tap(11,x,y);tap(20,x,y);
  if(state==0)sp_dirty=true;else if(state==1)paper_token=987;else if(state==2)surface.frame=1;
#ifdef PORTABLE_RASTER_SNAPSHOT
  else if(state==3)raster_sealed=true;else raster_recording=true;
#endif
  out=raw_at(10);assert(!out.tapped&&!out.buttons&&sv_need_neutral&&!sv_gesture);
  out=raw_at(11);assert(!out.tapped&&!out.buttons);
  /* Physical Back still reaches its owner while the contact gate is shut. */
  navigation_pending=T5_APP_BUTTON_BACK;direct_sample((portable_touch_sample){.valid=true},&out);assert(out.buttons&T5_APP_BUTTON_BACK);
  sp_dirty=false;paper_token=0;surface.frame=0;
#ifdef PORTABLE_RASTER_SNAPSHOT
  raster_sealed=raster_recording=false;
#endif
  /* A contact begun pending stays cancelled after the page becomes ready. */
  out=raw_at(12);assert(!out.tapped&&!out.buttons);
  out=raw_at(20);assert(!out.tapped&&!out.buttons);
  out=raw_at(21);assert(out.tapped&&out.touch_x==(int)x&&out.touch_y==(int)y);
 }
 runtime_exit=true;sp_dirty=true;assert(poll(&out,20)&&out.exit_requested&&(out.buttons&T5_APP_BUTTON_BACK));runtime_exit=false;sp_dirty=false;
}
int main(int argc,char**argv){
 assert(argc==4||argc==5);test_width=(unsigned)atoi(argv[1]);test_height=(unsigned)atoi(argv[2]);test_case=(unsigned)atoi(argv[3]);if(argc==5)frame_directory=argv[4];
 test_stride=test_width*2+6;test_size=test_stride*test_height;guarded_frame=malloc(test_size+64);assert(guarded_frame);memset(guarded_frame,0xa5,test_size+64);framebuffer=(uint16_t*)(guarded_frame+32);native_setup();
 if(test_case==0){
  for(unsigned deep=0;deep<2;deep++)for(unsigned high=0;high<2;high++){
   timer_deep=deep;timer_choice=high?3600000:deep?60000:5000;timer_message="";sv_switch(SV_TIMER);assert(portable_paper_frame_drain());geometry();char name[32];snprintf(name,sizeof(name),"%s-%s",deep?"deep":"idle",high?"max":"min");capture(name);
  }
  timer_message="Save unconfirmed - retry";settings_view_redraw();assert(portable_paper_frame_drain());capture("save-error");
  timer_message="Invalid: default shown";settings_view_redraw();assert(portable_paper_frame_drain());capture("invalid-record");
  /* Unbounded caller strings are clipped within a complete native list row. */
  const char *long_label="A very long settings label that exceeds the available width on every qualified display and must truncate";
  const char *long_value="Unconfirmed preference value with a very long explanation that must stay inside its own line";
  clear_color(0);sn_list_row((sn_rect){(int)margin(),(int)top(),(int)(test_width-2*margin()),test_height<200?32:44},long_label,long_value,true,true);present(false);
  assert(portable_paper_frame_drain());capture("long-labels");
  for(unsigned y=top();y<top()+(test_height<200?32u:44u);y++)for(unsigned x=0;x<test_width;x++)
   if(x<margin()||x>=test_width-margin())assert(!pixel(x,y));
  for(unsigned row=0;row<SETTINGS_ROW_COUNT;row++){
   settings_render(0,(int)row+1);assert(portable_paper_frame_drain());guard_check();
   if(row==SETTINGS_TIMER_ROW+1)capture("timer-rows");
  }
 }else if(test_case>=1&&test_case<=4){
  bool deep=(test_case-1)/2,high=(test_case-1)%2;unsigned step=deep?60000:5000,start=high?3600000:step;seed(deep,start);unsigned n=low_writes[deep?2:1];
  assert(choose(deep,high?1:-1,false)==T5_APP_SETTING_UPDATED);loaded_is(deep,start);assert(low_writes[deep?2:1]==n);
  assert(choose(deep,high?-1:1,false)==T5_APP_SETTING_UPDATED);loaded_is(deep,high?start-step:start+step);assert(low_writes[deep?2:1]==n+1);
  unsigned saved=high?start-step:start+step;assert(choose(deep,high?-1:1,true)==T5_APP_SETTING_NO_CHANGE);loaded_is(deep,saved);assert(low_writes[deep?2:1]==n+1);
  cleanup();native_setup();loaded_is(deep,saved);assert(choose(deep,0,true)==T5_APP_SETTING_NO_CHANGE);loaded_is(deep,saved);
 }else if(test_case==5||test_case==6){
  bool deep=test_case==6;unsigned start=deep?60000:20000;seed(deep,start);low_write_fail=deep?2:1;
  assert(choose(deep,1,false)==T5_APP_SETTING_NO_CHANGE);loaded_is(deep,start);assert(!strcmp(timer_message,"Save unconfirmed - retry"));
  low_write_fail=-1;assert(choose(deep,1,false)==T5_APP_SETTING_UPDATED);loaded_is(deep,start+(deep?60000:5000));
 }else if(test_case==7){gate_test();}
 else if(test_case==10){
  const risc_key_value_v1 *saved=settings_store;settings_store=NULL;
  assert(choose(false,1,false)==T5_APP_SETTING_NO_CHANGE);assert(!strcmp(timer_message,"Save unconfirmed - retry")&&!low_writes[1]);
  settings_store=saved;assert(choose(false,1,false)==T5_APP_SETTING_UPDATED);loaded_is(false,65000);
 }
 else if(test_case==8||test_case==9){
  bool deep=test_case==9;unsigned index=deep?2:1;memset(low_records[index],0xff,5);low_sizes[index]=5;
  assert(choose(deep,0,true)==T5_APP_SETTING_NO_CHANGE);assert(!strcmp(timer_message,"Invalid: default shown")&&!low_writes[index]);
  assert(choose(deep,0,false)==T5_APP_SETTING_UPDATED);loaded_is(deep,deep?300000:60000);
 }else assert(!"unknown case");
 assert(!writes&&!format_writes&&!kv_writes);cleanup();guard_check();free(guarded_frame);
 printf("{\"size\":[%u,%u],\"case\":%u,\"result\":\"passed\",\"frames\":%u,\"idle_writes\":%u,\"deep_writes\":%u}\n",test_width,test_height,test_case,displays,low_writes[1],low_writes[2]);return 0;
}
