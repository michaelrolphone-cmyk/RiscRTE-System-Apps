#define main old_alert_fixture_main
#include "portable_alarm_settings_test.c"
#undef main
static void capture(const char *dir,unsigned index) {
    char path[512];snprintf(path,sizeof(path),"%s/settings-%02u.ppm",dir,index);
    FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n240 240\n255\n");
    for(unsigned i=0;i<240*240;i++){unsigned p=framebuffer[i];unsigned char rgb[]={(p>>11)*255/31,((p>>5)&63)*255/63,(p&31)*255/31};assert(fwrite(rgb,1,3,f)==3);}assert(!fclose(f));
}
int main(int argc,char **argv) {
    assert(argc==3);scenario=100;unsigned mode=(unsigned)atoi(argv[2]);assert(mode<=1);
    assert(app_module_init()==0);settings_render(0,0);capture(argv[1],0);
    time_format_mode=mode;
    /* A real vertical touch drag exposes Hour; then plus and AM/PM, nested Back,
     * and Cancel run through the existing controller and actual adapter. */
    tap(3,120,160);tap(4,120,60);tap(8,120,96);tap(12,200,96);
    if(!mode)tap(16,120,160);
    tap(20,120,210);tap(24,60,210);
    assert(settings_activate(0,0)==T5_APP_SETTING_NO_CHANGE);
    assert(settings_draft.hour==(mode?13:1));assert(!rtc_writes && !kv_writes);
    settings_draft=(twatch_rtc_time_v1){2026,10,4,0,0,5,0};editor_field=3;
    sv_switch(SV_VALUE);capture(argv[1],1);settings_draft.hour=12;settings_view_redraw();capture(argv[1],2);
    sv_switch(SV_FIELDS);capture(argv[1],3);sv_switch(SV_TIME_FORMAT);capture(argv[1],4);
    sv_switch(SV_ALERT);capture(argv[1],5);alert_message="Save unconfirmed - retry";settings_view_redraw();capture(argv[1],6);
#ifdef PORTABLE_SLEEP_SETTINGS
    sv_switch(SV_SLEEP);capture(argv[1],7);
#endif
    assert(sv_inside(20,64,28,66,184,30));assert(sv_inside(115,107,28,66,184,30));
    assert(!sv_inside(116,107,28,66,184,30));assert(!sv_inside(115,108,28,66,184,30));
    app_module_fini();assert(!grants && !subscriptions && !frames && !rtc_writes && !kv_writes);
    puts("Nova Settings real drag/hour/AMPM/cancel, 44px bounds and actual pages passed");return 0;
}
