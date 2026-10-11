#define main baseline_fixture_main
#include "host_test.c"
#undef main
#include "RiscSceneComponentsV1.h"
#include <time.h>
static double cpu_us(void){struct timespec t;assert(!clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t));return t.tv_sec*1000000.0+t.tv_nsec/1000.0;}
int main(int argc,char **argv){
 assert(argc==2);poll_step=0;startup();assert(!api->close(NULL,session));
 const risc_scene_components_api_v1 *components=risc_scene_components_get_v1(api);assert(components);
 risc_components_document_v1 d={.api_version=1,.struct_size=sizeof(d),.revision=1,.root=1,.route_count=1,.node_count=60,.screen_key=1};
 d.routes[0]=(risc_scene_route_v1){1,99,"CHECKLIST"};
 for(unsigned i=0;i<60;i++){d.nodes[i]=(risc_scene_node_v1){.id=i+1,.route=1,.kind=RISC_COMPONENT_CHECK_ROW,.action=10,.maximum=1,.step=1};snprintf(d.nodes[i].label,40,"TASK %u WITH A LONG TITLE",i+1);d.details[i].secondary_action=20;d.details[i].marker=6;}
 expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;assert(!components->open(NULL,&d,NULL,&session));
 unsigned total_ticks=0,max_ticks=0;double total_us=0,max_tick_us=0;
 for(unsigned frame=0;frame<100;frame++){
  if(frame){++d.revision;d.nodes[0].value=frame&1;assert(!components->update(NULL,session,&d));}
  unsigned before=submits,ticks=0;double start=cpu_us();
  while(submits==before){risc_scene_event_v1 e;double one=cpu_us();assert(tick(&e)==RISC_SCENE_IDLE);double took=cpu_us()-one;if(took>max_tick_us)max_tick_us=took;ms+=10;assert(++ticks<1000);}
  total_us+=cpu_us()-start;total_ticks+=ticks;if(ticks>max_ticks)max_ticks=ticks;
  settle();
 }
 FILE*f=fopen(argv[1],"wb");assert(f);assert(fwrite(pixels+16,1,240*240*2,f)==240*240*2);assert(!fclose(f));
 printf("{\"frames\":100,\"ticks_per_frame\":%.2f,\"max_ticks\":%u,\"scheduled_ms_per_frame\":%.2f,\"host_cpu_us_per_frame\":%.2f,\"host_max_tick_cpu_us\":%.2f}\n",total_ticks/100.0,max_ticks,total_ticks/10.0,total_us/100,max_tick_us);
 finish();return 0;
}
