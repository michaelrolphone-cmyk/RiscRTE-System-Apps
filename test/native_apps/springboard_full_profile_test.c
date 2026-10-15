/* Full deployment feature profile; timing is host CPU only, not hardware FPS. */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <time.h>
uint64_t profile_calls[16];
#define main healthy_fixture_main
#define controller_tests healthy_controller_tests
#include PROFILE_FIXTURE
#undef main
#undef controller_tests
#include "springboard_native_checks.inc"
static uint64_t cost[8];
static unsigned calls[8];
static uint64_t stamp(void){struct timespec t;assert(!clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t));return (uint64_t)t.tv_sec*1000000000ull+t.tv_nsec;}
static void pb(void){uint64_t t=stamp();production->begin();cost[0]+=stamp()-t;calls[0]++;}
static void pc(int x,int y,int r,uint32_t rgb,uint8_t a){uint64_t t=stamp();production->circle(x,y,r,rgb,a);cost[1]+=stamp()-t;calls[1]++;}
static bool pi(int x,int y,int r,const char*n,uint8_t a){uint64_t t=stamp();bool ok=production->icon(x,y,r,n,a);cost[2]+=stamp()-t;calls[2]++;return ok;}
static void pt(int y,const char*s,bool c,uint32_t rgb){uint64_t t=stamp();production->caption(y,s,c,rgb);cost[3]+=stamp()-t;calls[3]++;}
static const t5_app_api_v1 *base_api;
static void pp(bool full){uint64_t t=stamp();base_api->present(full);cost[4]+=stamp()-t;calls[4]++;}
int main(int argc,char**argv){
 assert(argc==4);fw=(unsigned)atoi(argv[1]);fh=(unsigned)atoi(argv[2]);directory=strcmp(argv[3],"-")?argv[3]:NULL;pitch=fw+3;use_async=true;
 /* These are actual Waveshare unknown-timing capabilities, not fabricated FPS. */
 refresh_millihz=present_latency_us=0;
 extended_contexts=(contexts_fingerprint_service_v1){.base=ctx_api,.fingerprint_abi=0x31504643u,.fingerprint=healthy_fingerprint};extended_contexts.base.struct_size=sizeof(extended_contexts);
 guarded=malloc((fh*pitch+2)*2);assert(guarded);pixels=guarded+1;
 for(unsigned i=0;i<fh*pitch+2;i++)guarded[i]=0xa55a;
 if(getenv("VERIFY_NATIVE")) {
  const char *metadata=getenv("VERIFY_METADATA");if(metadata)select_metadata(metadata);
  if(expect_presentation){controller_tests();application_tests();}else fallback_tests();
  free(guarded);assert(!grants&&!allocation_count);
  printf("NATIVE_PASS %ux%u metadata=%s catalog=24 physical_contact=exact features=full lifecycle=clean\n",fw,fh,metadata?metadata:"unknown-timing");return 0;
 }
 open_controller();expect_transition=false;
 springboard_presentation p=*production;p.begin=pb;p.circle=pc;p.icon=pi;p.caption=pt;
 t5_app_api_v1 a=*api;base_api=api;a.present=pp;api=&a;
 nova_state s={.view=&p,.pending=-1,.initial=true};nova_layout(&s);s.time_known=true;s.hour=10;s.minute=21;
 uint64_t hash=1469598103934665603ull,state_hash=1469598103934665603ull;
 for(unsigned n=0;n<240;n++){
  if(n==120){s.first=19;nova_layout(&s);}
  unsigned phase=n%120;
  bool down=phase<60;
  uint64_t t=stamp();springboard_contact c=read_contact(down,physical(60+(phase<60?phase:59)),physical((unsigned)logical_height()/2));cost[5]+=stamp()-t;calls[5]++;
  t=stamp();nova_contact_input(&s,&c,40);nova_tick(&s,40);cost[6]+=stamp()-t;calls[6]++;
  ticks+=40;t=stamp();nova_render(&s);cost[7]+=stamp()-t;calls[7]++;guards();
  for(unsigned y=0;y<fh;y++)for(unsigned x=0;x<fw;x++){hash^=pixels[y*pitch+x];hash*=1099511628211ull;}
  const int v[]={(int)(s.motion.x*65536),(int)(s.motion.y*65536),(int)(s.motion.vx*65536),(int)(s.motion.vy*65536),(int)s.first,(int)s.focus,s.pending,s.pulse,(int)s.down};
  for(unsigned i=0;i<sizeof(v)/sizeof(v[0]);i++){state_hash^=(uint32_t)v[i];state_hash*=1099511628211ull;}
  if(n==0||n==30||n==59||n==119||n==120||n==180){char name[32];snprintf(name,sizeof(name),"frame-%03u",n);capture(name);}
 }
 const char *labels[]={"clear","circles","icons","mask_captions","submit_copy","input_poll","motion","render"};
 printf("PROFILE {\"width\":%u,\"height\":%u,\"frames\":240,",fw,fh);
 for(unsigned i=0;i<8;i++)printf("\"%s_us_per_frame\":%.3f,\"%s_calls\":%u,",labels[i],cost[i]/240000.0,labels[i],calls[i]);
 printf("\"pixels_hash\":\"%016llx\",\"state_hash\":\"%016llx\",\"instrument_calls\":[",(unsigned long long)hash,(unsigned long long)state_hash);
 for(unsigned i=0;i<16;i++)printf("%s%llu",i?",":"",(unsigned long long)profile_calls[i]);
 printf("]}\n");
 close_controller();free(guarded);assert(!grants&&!allocation_count);return 0;
}
