/* Production app + real adapter touch routing; only the hardware is modeled. */
#define grants fixture_grants
#define main capture_fixture_main
#include "nova_peripherals.h"
#undef main
#undef grants
#include NOVA_APP_SOURCE
static void touch_at(unsigned at,int x,int y) {actions[action_count].at=at;actions[action_count].x=x;actions[action_count++].y=y;}
int main(int argc,char **argv) {
    assert(argc==3);directory=argv[1];unsigned test_case=(unsigned)atoi(argv[2]);
    memset(pixels,0xa5,sizeof(pixels));stop_poll=400;
#if NOVA_APP_ID == 1
    touch_at(20,40,74);touch_at(40,199,212);touch_at(60,92,166);touch_at(80,145,212);
    if(test_case){touch_at(100,210,26);touch_at(120,120,152);}
#elif NOVA_APP_ID == 2
    touch_at(20,48,204);touch_at(60,48,204);
    if(test_case){touch_at(100,120,204);touch_at(140,120,204);}
#elif NOVA_APP_ID == 3
    touch_at(20,200,206);touch_at(40,200,206);touch_at(60,200,206);
    if(test_case)touch_at(80,120,206);
#elif NOVA_APP_ID == 4
    if(test_case){const uint8_t record[]={0x54,1,1,0xa4};fake_put(NULL,"time_format",record,4);}
    touch_at(20,110,82);touch_at(40,120,80);touch_at(60,200,118);
    if(!test_case)touch_at(80,66,206);
    touch_at(100,test_case?120:174,206);touch_at(120,30,26);touch_at(140,50,206);
#elif NOVA_APP_ID == 5
    touch_at(20,110,82);touch_at(40,120,180);touch_at(60,200,118);
    touch_at(100,120,206);touch_at(120,30,26);touch_at(140,50,206);
    if(test_case)touch_at(180,174,206);
#endif
    assert(app_module_init()==0);app_main();
#if NOVA_APP_ID == 1
    assert(!strcmp(calculator_state.text,test_case?"-9":"9"));assert(!calculator_menu);
#elif NOVA_APP_ID == 2
    assert(!clock_state.running);assert(test_case?clock_state.elapsed_ms==0:clock_state.elapsed_ms>0);
#elif NOVA_APP_ID == 3
    assert(selected_index==3);
#elif NOVA_APP_ID == 4
    assert(writer.saved.enabled && !writer.uncertain && writer.saved.revision==1 && alarm_page==0);
    twatch_rtc_time_v1 raw,local;assert(deadline_time(writer.saved.deadline,&raw) && portable_time_forward(&raw,&local));
    assert(local.hour==(test_case?7:19) && local.minute==35);
    assert(alarm_time_format==(test_case?PORTABLE_TIME_FORMAT_24:PORTABLE_TIME_FORMAT_12));
#elif NOVA_APP_ID == 5
    assert(!writer.uncertain && alarm_page==0 && writer.saved.revision==(test_case?2u:1u));
    assert(test_case?!writer.saved.enabled:writer.saved.enabled && writer.saved.duration==301);
#endif
    app_module_fini();assert(!fixture_grants && !frames && !subs);
    printf("Nova real-adapter app %d case %u: actual touches, model, frame stride and cleanup passed\n",NOVA_APP_ID,test_case);
    return 0;
}
