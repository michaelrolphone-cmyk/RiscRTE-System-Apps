#define PORTABLE_SETTINGS_AVAILABILITY
#define main original_catalog_main
#include "home_catalog_render_test.c"
#undef main
static const char *status_notice="";
static uint64_t status_generation=1;
static unsigned status_polls,config_acquires,config_releases,now_ms;
static uint32_t clock_now(void){return now_ms;}
static int32_t config_get(void *c,const char *key,void *out,uint32_t cap,uint32_t *size) {
 (void)c;*size=0;
 if(!strcmp(key,"time_format")){assert(cap>=4);const uint8_t bytes[]={0x54,1,1,0xa4};memcpy(out,bytes,4);*size=4;return 0;}
 return RISC_KEY_VALUE_NOT_FOUND;
}
static int32_t config_put(void *c,const char *key,const void *in,uint32_t size){(void)c;(void)key;(void)in;(void)size;assert(!"No write on reload");return RISC_KEY_VALUE_CONTEXT;}
static const risc_key_value_v1 config_api={1,sizeof(config_api),NULL,config_get,config_put};
static bool config_acquire(const char *name,uint32_t version,uint64_t ns,risc_runtime_capability_v1 *out) {
 if(strcmp(name,RISC_KEY_VALUE_CAPABILITY))return false;
 assert(version==1&&ns==1);++config_acquires;*out=(risc_runtime_capability_v1){sizeof(*out),1,1,&config_api};return true;
}
static bool config_release(risc_runtime_capability_v1 *grant){assert(grant->api==&config_api);++config_releases;*grant=(risc_runtime_capability_v1){.struct_size=sizeof(*grant)};return true;}
static const risc_runtime_api_v1 config_runtime={.api_version=1,.struct_size=sizeof(config_runtime),.acquire=config_acquire,.release=config_release};
void portable_desk_adapter_settings_pending(bool pending){assert(!pending);}
const char *portable_home_settings_notice(void){return status_notice;}
bool portable_home_settings_poll(void){++status_polls;return true;}
bool portable_home_settings_available(void){return !*status_notice;}
uint64_t portable_home_settings_generation(void){return status_generation;}
int main(int argc,char **argv) {
 int result=original_catalog_main(argc,argv);assert(!result);
 const char *notices[]={"SETTINGS UNAVAILABLE; RAM DEFAULTS","SETTINGS PENDING; RESTART RUNTIME","SETTINGS CONFLICT; SOURCES KEPT"};
 const char *names[]={"settings-unavailable","settings-pending","settings-conflict"};
 twatch_rtc_time_v1 time={.year=2026,.month=10,.day=7,.weekday=3,.hour=9,.minute=22};
 for(unsigned i=0;i<3;++i) {
  status_notice=notices[i];assert(home_text_width(&HOME_R600_22,status_notice,0)<=416*64);
  cache_check(&time,true,"");cache_check(&time,false,"");save(argv[1],names[i]);
 }
 t5_app_api_v1 timed_app=*app;timed_app.millis=clock_now;app=&timed_app;rt=&config_runtime;
 desk_promoted=true;desk_boot_timer=desk_held=desk_retained=desk_config_pending=false;
 desk_settings_generation=desk_settings_observed=0;desk_settings_poll_at=0;format=0;
 status_notice=notices[0];assert(desk_hydrate()&&!config_acquires&&format==0);
 now_ms+=1000;status_notice=notices[1];++status_generation;assert(desk_hydrate()&&!config_acquires&&format==0);
 now_ms+=1000;status_notice="";++status_generation;assert(desk_hydrate());
 assert(config_acquires==1&&config_releases==1&&format==1&&desk_settings_generation==status_generation);
 now_ms+=1000;assert(!desk_hydrate()&&config_acquires==1);
 desk_boot_timer=true;unsigned polls_before=status_polls;now_ms+=1000;++status_generation;
 assert(!desk_hydrate()&&status_polls==polls_before&&config_acquires==1);
 desk_boot_timer=false;desk_held=true;assert(!desk_hydrate()&&status_polls==polls_before);
 puts("Home clean foreground generation reload: one complete read, no writes, no timer/held discovery");
 puts("Home settings notices: actual scene cache and raster, all status captions fit, unchanged scenes retained");return 0;
}
