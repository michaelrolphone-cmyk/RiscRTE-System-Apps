#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "PortableHomePower.h"
#include "PortableHomeContextReadiness.h"
static char order[128];static unsigned n;static int fault,hold_rc,entry_rc;static bool pending;
static portable_home_power state;
static unsigned lose_after;
static bool call_safe(void*p){(void)p;return !lose_after||n<lose_after;}
static void step(char c){assert(!state.retained);order[n++]=c;order[n]=0;}
static bool panel_prepare(void*p){(void)p;step('P');assert(!state.held);return fault!=1;}
static bool power_prepare(void*p){(void)p;step('U');assert(!state.held);return fault!=2;}
static bool power_resume(void*p){(void)p;step('R');return fault!=3;}
static int32_t panel_resume(void*p){(void)p;step('S');return fault==4?RISC_DEEP_SLEEP_PLATFORM:0;}
static int32_t hold(void*p){(void)p;step('H');assert(!state.held);return hold_rc;}
static bool wake(void*p,bool*out){(void)p;step('K');assert(state.held);*out=pending;return fault!=5;}
static int32_t deep(void*p,uint32_t ms){(void)p;step('E');assert(state.held&&ms==1000);return entry_rc;}
static int32_t deep_set(void*p,uint32_t ms){(void)p;step('W');assert(state.held&&ms==1000);return entry_rc;}
static int32_t light(void*p,uint32_t ms,risc_light_sleep_result_v1*out){(void)p;(void)ms;(void)out;assert(!"Applicability must never call a provider");return 0;}
static risc_display_power_api_v1 display={.api_version=1,.struct_size=sizeof(display),.prepare_sleep=panel_prepare,.prepare_deep_sleep=hold,.resume_status=panel_resume};
static risc_power_sleep_api_v1 power={.api_version=1,.struct_size=sizeof(power),.prepare_sleep=power_prepare,.resume=power_resume,.sleep_wake_pending=wake,.deep_sleep_for=deep,.deep_sleep_set=deep_set};
static void reset(void){state=(portable_home_power){.display=&display,.power=&power};n=0;lose_after=0;order[0]=0;fault=0;hold_rc=0;entry_rc=RISC_DEEP_SLEEP_BUSY;pending=false;}
static void prepared(void){assert(portable_home_power_prepare(&state)==1);step('A');assert(!state.held);assert(portable_home_power_hold(&state)==1);}
int main(void){
 for(unsigned stop=1;stop<=2;stop++){
  reset();state.call_safe=call_safe;lose_after=stop;
  assert(portable_home_power_prepare(&state)==-2&&n==stop);
  assert(portable_home_power_restore(&state)==-2&&n==stop);
 }
 reset();state.call_safe=call_safe;assert(portable_home_power_prepare(&state)==1);
 lose_after=3;assert(portable_home_power_hold(&state)==-2&&n==3);
 reset();state.call_safe=call_safe;prepared();lose_after=n+1;
 assert(portable_home_power_enter(&state,1000)==-2&&!strcmp(order,"PUAHK"));
 reset();state.call_safe=call_safe;prepared();lose_after=n+1;
 assert(portable_home_power_restore(&state)==-2&&!strcmp(order,"PUAHR"));

 portable_home_readiness applicability={.power_modes=PORTABLE_HOME_MODE_LIGHT,.sleep_settings=true,.dnd=true};
 assert(portable_home_readiness_query(&applicability,RISC_CONTEXTS_SETTING_QUERY(RISC_CONTEXTS_SLEEP))==1);
 assert(portable_home_readiness_query(&applicability,RISC_CONTEXTS_SETTING_QUERY(RISC_CONTEXTS_DEEP))==0);
 assert(portable_home_readiness_query(&applicability,RISC_CONTEXTS_SETTING_OPTION_QUERY(RISC_CONTEXTS_POWER,0))==0);
 assert(portable_home_readiness_query(&applicability,RISC_CONTEXTS_SETTING_OPTION_QUERY(RISC_CONTEXTS_POWER,1))==1);
 assert(portable_home_readiness_query(&applicability,RISC_CONTEXTS_SETTING_OPTION_QUERY(RISC_CONTEXTS_NOTIFY,0))==1);
 assert(portable_home_readiness_query(&applicability,RISC_CONTEXTS_SETTING_OPTION_QUERY(RISC_CONTEXTS_NOTIFY,1))==0);
 applicability.sound=true;assert(portable_home_readiness_query(&applicability,RISC_CONTEXTS_SETTING_OPTION_QUERY(RISC_CONTEXTS_NOTIFY,2))==1);
 assert(portable_home_readiness_query(&applicability,RISC_CONTEXTS_SETTING_OPTION_QUERY(RISC_CONTEXTS_NOTIFY,3))==0);
 assert(portable_home_readiness_query(&applicability,RISC_CONTEXTS_SETTING_OPTION_QUERY(RISC_CONTEXTS_BRIGHTNESS,0))==-1);
 assert(portable_home_readiness_query(&applicability,0)==-1);

 reset();assert(portable_home_power_modes(&state,true)==PORTABLE_HOME_MODE_DEEP&&!n);
 power.light_sleep_for=light;assert(portable_home_power_modes(&state,true)==7&&!n);assert(portable_home_power_modes(&state,false)==PORTABLE_HOME_MODE_LIGHT&&!n);
 state.enrolled_wake_sources=true;assert(portable_home_power_modes(&state,true)==PORTABLE_HOME_MODE_DEEP&&!n);
 power.light_sleep_set=light;assert(portable_home_power_modes(&state,true)==7&&!n);
 power.struct_size=RISC_POWER_TIMED_SLEEP_SIZE;assert(portable_home_power_modes(&state,true)==0&&!n);
 state.enrolled_wake_sources=false;assert(portable_home_power_modes(&state,true)==PORTABLE_HOME_MODE_LIGHT&&!n);
 power.struct_size=sizeof(power);state.retained=true;assert(portable_home_power_modes(&state,true)==0&&!n);
 state.retained=false;display.resume_status=NULL;assert(portable_home_power_modes(&state,true)==0&&!n);display.resume_status=panel_resume;
 power.light_sleep_for=power.light_sleep_set=NULL;

 reset();prepared();assert(portable_home_power_enter(&state,1000)==0);assert(portable_home_power_restore(&state)==1);assert(!strcmp(order,"PUAHKERS")&&!state.held);
 reset();state.enrolled_wake_sources=true;prepared();assert(portable_home_power_enter(&state,1000)==0);assert(portable_home_power_restore(&state)==1);assert(!strcmp(order,"PUAHKWRS"));
 reset();state.display=NULL;assert(!portable_home_power_supported(&state)&&portable_home_power_prepare(&state)==0&&!n);
 reset();display.struct_size=RISC_DISPLAY_POWER_LIGHT_SLEEP_SIZE;assert(!portable_home_power_supported(&state)&&portable_home_power_prepare(&state)==0&&!n);display.struct_size=sizeof(display);
 reset();display.resume_status=NULL;assert(!portable_home_power_supported(&state)&&portable_home_power_prepare(&state)==0&&!n);display.resume_status=panel_resume;
 reset();display.api_version=2;assert(!portable_home_power_supported(&state)&&portable_home_power_prepare(&state)==0&&!n);display.api_version=1;
 reset();state.enrolled_wake_sources=true;power.struct_size=RISC_POWER_TIMED_DEEP_SLEEP_SIZE;assert(!portable_home_power_supported(&state)&&portable_home_power_prepare(&state)==0&&!n);power.struct_size=sizeof(power);
 reset();power.struct_size=RISC_POWER_TIMED_SLEEP_SIZE;assert(!portable_home_power_supported(&state)&&portable_home_power_prepare(&state)==0&&!n);power.struct_size=sizeof(power);
 reset();power.deep_sleep_for=NULL;assert(!portable_home_power_supported(&state)&&portable_home_power_prepare(&state)==0&&!n);power.deep_sleep_for=deep;
 reset();fault=1;assert(portable_home_power_prepare(&state)==0&&!strcmp(order,"PS"));
 reset();fault=2;assert(portable_home_power_prepare(&state)==0&&!strcmp(order,"PURS"));
 reset();prepared();fault=3;assert(portable_home_power_restore(&state)==-2&&!strcmp(order,"PUAHR"));unsigned stop=n;assert(portable_home_power_restore(&state)==-2&&portable_home_power_enter(&state,1000)==-2&&n==stop);
 reset();prepared();fault=4;assert(portable_home_power_restore(&state)==-2&&!strcmp(order,"PUAHRS"));
 for(int rc=RISC_DEEP_SLEEP_UNSUPPORTED;rc<=1;rc++){
  reset();hold_rc=rc;assert(portable_home_power_prepare(&state)==1);int result=portable_home_power_hold(&state);
  if(rc==0){assert(result==1);assert(portable_home_power_restore(&state)==1);}
  else if(rc==RISC_DEEP_SLEEP_RETAINED||rc>0){assert(result==-2&&state.retained&&!strcmp(order,"PUH"));}
  else assert(result==0&&!state.held&&!strcmp(order,"PUHRS"));
 }
 reset();hold_rc=-99;assert(portable_home_power_prepare(&state)==1&&portable_home_power_hold(&state)==-2&&!strcmp(order,"PUH"));
 reset();prepared();pending=true;assert(portable_home_power_enter(&state,1000)==0&&!strcmp(order,"PUAHK"));assert(portable_home_power_restore(&state)==1);
 reset();prepared();fault=5;assert(portable_home_power_enter(&state,1000)==-2&&!strcmp(order,"PUAHK"));
 int entries[]={RISC_DEEP_SLEEP_RETAINED,0,1,-99};
 for(unsigned i=0;i<sizeof(entries)/sizeof(entries[0]);i++){reset();prepared();entry_rc=entries[i];assert(portable_home_power_enter(&state,1000)==-2&&!strcmp(order,"PUAHKE"));stop=n;assert(portable_home_power_restore(&state)==-2&&n==stop);}
 reset();prepared();assert(portable_home_power_enter(&state,0)==0&&portable_home_power_enter(&state,RISC_TIMED_SLEEP_MAX_MS+1u)==0&&!strcmp(order,"PUAH"));assert(portable_home_power_restore(&state)==1);
 puts("generic Home physical power: unavailable timed suffix, nonholding/alarm/hold ordering, enrolled wake, active input, ordinary rollback, terminal/unknown custody PASS");
 return 0;
}
