#include <dlfcn.h>
#include "PortableContextPreferences.h"
#include "ContextsServiceV1.h"
#include "ContextFingerprintService.h"
#include "RiscAppDataV1.h"
static int (*home_checkpoint)(uint32_t);
static int32_t (*home_get)(const char*,void*,uint32_t,uint32_t*);
static int32_t (*home_put)(const char*,const void*,uint32_t);
static unsigned (*home_brightness)(void);
static const void*(*home_alarm)(void);
static unsigned expected_writes,expected_calls;static bool radio_rule_case,rule_evaluated;
static contexts_fingerprint_service_v1 rules_service;
static unsigned preset_claims,preset_reports,preset_writes,settings_calls;
static int32_t composed_checkpoint(uint64_t token,const risc_resident_request_v1*q,risc_resident_reply_v1*r){
 assert(token==9&&!q->flags&&!q->reserved&&!files_ui.token&&!held&&!presenting);
 assert(preset_claims==1&&preset_reports==1&&preset_writes==expected_writes);
 ++settings_calls;r->flags=0;return home_checkpoint(q->reason);
}
static bool composed_resident(risc_resident_client_v1*c){uint32_t cap=c->struct_size;memset(c,0,cap);*c=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(*c),.invocation=9,.role=RISC_RESIDENT_ROLE_FOREGROUND,.checkpoint=composed_checkpoint};return true;}
static bool composed_info(void*c,risc_display_info_v1*out){bool ok=get_info(c,out);out->flags|=RISC_DISPLAY_INFO_BRIGHTNESS;return ok;}
static bool composed_brightness(void*c,uint16_t v,uint16_t max){(void)c;(void)v;(void)max;assert(0&&"Files must request Home physical effects");return false;}
static risc_display_output_api_v1 composed_display;
static int32_t preset_get(const char*k,void*b,uint32_t cap,uint32_t*n){
 if(!strcmp(k,"ctx_p0")){assert(cap>=64);portable_context_preset p;portable_context_preset_init(&p);p.enabled=true;p.source=CONTEXTS_AUDIO;p.slot=0;p.actions=PORTABLE_CONTEXT_BRIGHTNESS|(expected_writes==2?PORTABLE_CONTEXT_VOLUME:0);p.brightness=70;p.volume=50;strcpy(p.name,"office");assert(portable_context_preset_encode(&p,b));*n=64;return 0;}
 return home_get(k,b,cap,n);
}
static int32_t rules_stat(void*c,const char*n,uint32_t*size,uint64_t*revision){(void)c;(void)n;*size=0;*revision=0;return RISC_APP_DATA_NOT_FOUND;}
static int32_t rules_read(void*c,const char*n,uint64_t rev,void*b,uint32_t cap,uint32_t*size,uint64_t*actual){(void)c;(void)n;(void)rev;(void)b;(void)cap;(void)size;(void)actual;assert(0);return -1;}
static int32_t rules_write(void*c,const char*n,uint64_t rev,const void*b,uint32_t size){(void)c;(void)n;(void)rev;(void)b;(void)size;assert(0);return -1;}
static const risc_app_data_v1 rules_data={1,sizeof(rules_data),NULL,rules_stat,rules_read,rules_write};
static bool rules_fingerprint(void*c,uint32_t op,void*request){
 (void)c;
 if(op==CONTEXTS_FP_STATUS){contexts_fingerprint_status_v1*q=request;*q=(contexts_fingerprint_status_v1){.struct_size=sizeof(*q),.sources=CONTEXTS_ALL};return true;}
 if(op==CONTEXTS_FP_CONFIG||op==CONTEXTS_FP_RULES)return true;
 if(op==CONTEXTS_FP_EVALUATE){contexts_evaluate_v1*q=request;if(!rule_evaluated){rule_evaluated=true;++preset_claims;q->result.changed=(1u<<CR_WIFI)|(1u<<CR_BLUETOOTH);q->result.winners[CR_WIFI]=q->result.winners[CR_BLUETOOTH]=0;q->result.settings[CR_WIFI]=0;q->result.settings[CR_BLUETOOTH]=1;}return true;}
 if(op==CONTEXTS_FP_WORKFLOW){contexts_workflow_v1*q=request;if(!q->claim){++preset_reports;assert(q->result==CR_APPLIED&&preset_writes==2&&!settings_calls);}return true;}
 assert(0);return false;
}
static void child_run(void){char*args[]={"files","contexts","watch"};assert(files_child_main(3,args)==0);assert(preset_claims==1&&preset_reports==1&&preset_writes==expected_writes&&settings_calls==expected_calls&&(radio_rule_case||home_brightness()==70));}
int main(int argc,char**argv){
 assert(argc==3);radio_rule_case=!strcmp(argv[2],"radios");expected_writes=!strcmp(argv[2],"brightness")?1:2;expected_calls=radio_rule_case?6:!strcmp(argv[2],"brightness")?3:!strcmp(argv[2],"pending")?10:7;void*module=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!module){fprintf(stderr,"%s\n",dlerror());abort();}
 int(*start)(void(*)(void),const char*)=dlsym(module,"files_home_start");home_checkpoint=dlsym(module,"files_home_checkpoint");home_get=dlsym(module,"files_home_get");home_put=dlsym(module,"files_home_put");home_brightness=dlsym(module,"files_home_brightness");home_alarm=dlsym(module,"files_home_alarm");assert(home_alarm&&start&&home_checkpoint&&home_get&&home_put&&home_brightness);
 assert(!start(child_run,argv[2]));assert(!dlclose(module));puts("Full Files root/background + production scene + full Home settings controller: preset persistence/report before one physical brightness effect PASS");return 0;
}
