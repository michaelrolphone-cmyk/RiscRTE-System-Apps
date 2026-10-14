/* Real text owner + scene reducer; independently timed physical edges.
 * No wait for presentation. Coordinates come from the accepted grid reference. */
#define main unused_transport_fixture_main
#include "../scene/host_test.c"
#undef main
#include "RiscTextEntryV1.h"
extern const risc_driver_v2 *typing_text_driver_get(uint32_t);
int main(int argc,char **argv){
 assert(argc==7);const char *profile=argv[1];unsigned period=atoi(argv[2]),delay=atoi(argv[3]),repeat=atoi(argv[4]),count=200;
 if(!strcmp(profile,"x4")){w=800;h=480;format=1;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,65535,0,270,0,0};}
 else if(!strcmp(profile,"wave")){w=410;h=502;}
 else assert(!strcmp(profile,"watch"));
 poll_step=0;presentation_delay=delay;expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;
 driver=t5_driver_get(2);api=driver->capability;
 risc_provider_dependency_v1 deps[]={{"display.output",1,&disp},{"input.touch.raw",1,&raw},{"input.navigation",1,&nav},{"platform.clock",1,&clk},{"ui.presentation-profile",1,&prof}};
 assert(driver->start(deps,5));const risc_driver_v2 *td=typing_text_driver_get(2);const risc_text_entry_api_v1 *text=td->capability;
 const risc_text_entry_api_v1_long *longtext=risc_text_entry_long(text);assert(longtext);
 risc_provider_dependency_v1 dep={"ui.scene",1,api};assert(td->start(&dep,1));
 risc_text_entry_request_v1_long q={.api_version=1,.struct_size=sizeof(q),.capacity=256,.label="SENSOR NAME"};
 if(!strcmp(argv[5],"style")){strcpy(q.text,"Coffee 42");count=0;}
 uint64_t token;assert(!longtext->open_long(NULL,&q,&token));
 unsigned received=0;char expected[256]={0};risc_text_entry_state_v1_long result={0};
 unsigned lw=w==800?480:w,lh=w==800?800:h;
 for(unsigned t=0;t<period*count+40;t++){
  ms=t;unsigned i=t/period;
  if(i<count&&(t%period==0||t%period==20)){
   assert(event_count==event_at);event_count=1;event_at=0;
   unsigned x=(repeat||i%2==0?52:94)*lw/480,y=(lh>=lw*3/2?412*lh/800:97*lh/240);
   held_input=t%period==0;
   events[0]=(risc_touch_event_v1){++seq,ms,held_input?1:3,0,x,y};
  }
  result=(risc_text_entry_state_v1_long){.struct_size=sizeof(result)};assert(!longtext->poll_long(NULL,token,&result));
  if(count&&strlen(result.text)!=received){
   assert(t==received*period+20);expected[received]=repeat||received%2==0?'Q':'W';++received;
   assert(!strcmp(expected,result.text));
  }
 }
 assert(received==count);
 for(unsigned i=0;;i++){assert(i<10000);ms+=10;result.struct_size=sizeof(result);assert(!longtext->poll_long(NULL,token,&result));if(!(result.flags&RISC_TEXT_ENTRY_PRESENTING))break;}
 FILE *f=fopen(argv[6],"wb");assert(f);unsigned bits=format==5?16:1;assert(fwrite(pixels+16,1,(w*bits+7)/8*h,f)==(w*bits+7)/8*h);fclose(f);
 int rc;while((rc=text->close(NULL,token))==RISC_TEXT_ENTRY_AGAIN)ms+=10;assert(!rc&&td->quiesce());td->stop();assert(driver->quiesce());driver->stop();
 printf("%s period=%u panel=%u repeat=%u received=%u exact_release_latency=0 PASS\n",profile,period,delay,repeat,received);
}
