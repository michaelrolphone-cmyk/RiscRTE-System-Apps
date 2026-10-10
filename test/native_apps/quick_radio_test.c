#include "PortableQuickRadios.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t record[4],bstate;static bool saved,rf_live,fail_read,fail_write,commit_io,fail_ble,fail_off,fail_release;static unsigned gets,puts_,disconnects,opens,closes,grants,releases;
static int32_t get(void*c,const char*k,void*out,uint32_t cap,uint32_t*n){(void)c;assert(!strcmp(k,PORTABLE_RADIO_KEY)&&cap==4);gets++;*n=0;if(fail_read)return RISC_KEY_VALUE_IO;if(!saved)return RISC_KEY_VALUE_NOT_FOUND;memcpy(out,record,4);*n=4;return RISC_KEY_VALUE_OK;}
static int32_t put(void*c,const char*k,const void*in,uint32_t n){(void)c;assert(!strcmp(k,PORTABLE_RADIO_KEY)&&n==4);puts_++;if(fail_write)return RISC_KEY_VALUE_IO;memcpy(record,in,4);saved=true;return commit_io?RISC_KEY_VALUE_IO:RISC_KEY_VALUE_OK;}
static const risc_key_value_v1 kv={1,sizeof(kv),NULL,get,put};
static wifi_link_t wstatus(void*c){(void)c;return rf_live?WIFI_LINK_UP:WIFI_LINK_DOWN;}
static bool off(void*c){(void)c;disconnects++;if(fail_off)return false;rf_live=false;return true;}
static const wifi_api_v1 w={.api_version=1,.struct_size=sizeof(w),.status=wstatus,.disconnect_checked=off};
static bool benable(void*c,bool on){(void)c;if(fail_ble)return false;if(on){if(bstate!=PORTABLE_BLUETOOTH_ON)opens++;bstate=PORTABLE_BLUETOOTH_ON;}else {if(bstate!=PORTABLE_BLUETOOTH_OFF)closes++;bstate=PORTABLE_BLUETOOTH_OFF;}return true;}
static bool bstatus(void*c,uint8_t*out){(void)c;*out=bstate;return true;}
static const portable_bluetooth_control_v1 b={.api_version=1,.struct_size=sizeof(b),.set_enabled=benable,.status=bstatus};
static bool acquire(const char*cap,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){assert(v==1);grants++;if(!strcmp(cap,RISC_KEY_VALUE_CAPABILITY)){assert(id==1);g->api=&kv;}else if(!strcmp(cap,"net.wifi")){assert(id==15);g->api=&w;}else {assert(!strcmp(cap,"bluetooth.hci")&&id==16);g->api=&b;}return true;}
static bool release(risc_runtime_capability_v1*g){assert(g->api);if(fail_release)return false;g->api=NULL;releases++;return true;}
static char logs[16384];
static bool health(risc_runtime_health_v1*out){out->uptime_ms=1234;return true;}
static bool diagnostic(const char*line){assert(strlen(logs)+strlen(line)+2<sizeof(logs));strcat(logs,line);strcat(logs,"\n");return true;}
static const risc_runtime_api_v1 rt={.api_version=1,.struct_size=sizeof(rt),.acquire=acquire,.release=release,.health=health,.diagnostic=diagnostic};
static void reset(void){logs[0]=0;memset(record,0,4);saved=rf_live=fail_read=fail_write=commit_io=fail_ble=fail_off=fail_release=false;bstate=0;gets=puts_=disconnects=opens=closes=grants=releases=0;}
int main(void){
 pqa_radios s;pqa_state u;reset();pqa_init(&u);assert(pqa_radios_load(&s,&u,&rt));assert(u.radios_valid&&u.wifi_enabled&&!u.bluetooth_enabled&&!u.airplane&&!puts_&&!opens);
#ifdef PORTABLE_STAGE_LOGS
 assert(strstr(logs,"APP t_ms=1234 stage=radio-preferences result=missing defaults=wifi-allowed,bluetooth-off"));
#else
 assert(!logs[0]);
#endif
 assert(pqa_radios_apply(&s,&u,&rt,PQA_BLUETOOTH));assert(u.bluetooth_enabled&&opens==1&&saved);assert(pqa_radios_apply(&s,&u,&rt,PQA_AIRPLANE));assert(u.airplane&&!u.wifi_enabled&&!u.bluetooth_enabled&&bstate==0&&closes==1);
#ifdef PORTABLE_STAGE_LOGS
 assert(strstr(logs,"stage=radio-save result=confirmed")&&strstr(logs,"stage=bluetooth-result state=on"));
#endif
 pqa_radios restored;pqa_state fresh;pqa_init(&fresh);assert(pqa_radios_load(&restored,&fresh,&rt));assert(fresh.airplane&&!fresh.wifi_enabled&&!fresh.bluetooth_enabled);
#ifdef PORTABLE_STAGE_LOGS
 assert(strstr(logs,"stage=radio-preferences result=persisted"));
#endif
 assert(pqa_radios_apply(&restored,&fresh,&rt,PQA_AIRPLANE));assert(!fresh.airplane&&fresh.wifi_enabled&&fresh.bluetooth_enabled&&bstate==1);
 assert(pqa_radios_suspend(&rt)&&bstate==0);unsigned writes=puts_;assert(pqa_radios_resume(&restored,&fresh,&rt)&&bstate==1&&puts_==writes);
 assert(pqa_radios_apply(&restored,&fresh,&rt,PQA_AIRPLANE));assert(pqa_radios_apply(&restored,&fresh,&rt,PQA_WIFI));assert(!fresh.airplane&&fresh.wifi_enabled&&!fresh.bluetooth_enabled&&bstate==0);
 assert(portable_radio_wifi_allowed(&rt));assert(pqa_radios_apply(&restored,&fresh,&rt,PQA_WIFI));assert(!portable_radio_wifi_allowed(&rt));assert(grants==releases);
 reset();pqa_init(&u);assert(pqa_radios_load(&s,&u,&rt));rf_live=true;fail_off=true;assert(pqa_radios_apply(&s,&u,&rt,PQA_WIFI));assert(u.wifi_enabled&&!saved&&rf_live&&(u.error_flags&PQA_ERROR_SAVE));
 fail_off=false;fail_write=true;assert(pqa_radios_apply(&s,&u,&rt,PQA_BLUETOOTH));assert(!u.bluetooth_enabled&&bstate==0&&!saved);
 fail_write=false;commit_io=true;assert(pqa_radios_apply(&s,&u,&rt,PQA_BLUETOOTH));assert(u.bluetooth_enabled&&bstate==1&&saved);
 fail_ble=true;assert(!pqa_radios_apply(&s,&u,&rt,PQA_AIRPLANE));assert(u.bluetooth_enabled&&(u.error_flags&PQA_ERROR_RADIO));
 reset();saved=true;memset(record,0xff,4);pqa_init(&u);assert(pqa_radios_load(&s,&u,&rt));assert(!u.radios_valid&&!portable_radio_wifi_allowed(&rt)&&!opens&&!puts_);
 reset();fail_read=true;pqa_init(&u);assert(pqa_radios_load(&s,&u,&rt)&&!u.radios_valid&&!puts_);
 reset();saved=rf_live=true;bstate=PORTABLE_BLUETOOTH_ON;memset(record,0xff,4);pqa_init(&u);
 assert(pqa_radios_load(&s,&u,&rt));assert(!u.radios_valid&&!rf_live&&bstate==PORTABLE_BLUETOOTH_OFF&&!puts_&&closes==1&&(u.error_flags&PQA_ERROR_RADIO));
 reset();rf_live=fail_read=true;bstate=PORTABLE_BLUETOOTH_ON;pqa_init(&u);
 assert(pqa_radios_load(&s,&u,&rt));assert(!u.radios_valid&&!rf_live&&bstate==PORTABLE_BLUETOOTH_OFF&&!puts_);
 reset();rf_live=fail_read=fail_off=true;bstate=PORTABLE_BLUETOOTH_ON;pqa_init(&u);
 assert(!pqa_radios_load(&s,&u,&rt));assert(!u.radios_valid&&rf_live&&bstate==PORTABLE_BLUETOOTH_ON&&!puts_);
 reset();rf_live=fail_read=fail_ble=true;bstate=PORTABLE_BLUETOOTH_RETAINED;pqa_init(&u);
 assert(!pqa_radios_load(&s,&u,&rt));assert(!u.radios_valid&&!rf_live&&bstate==PORTABLE_BLUETOOTH_RETAINED&&!puts_);
 reset();fail_release=true;pqa_init(&u);assert(!pqa_radios_load(&s,&u,&rt));
 for(unsigned flags=0;flags<256;flags++){uint8_t bytes[]={0x51,1,(uint8_t)flags,(uint8_t)(flags^0xa5)},out=0;bool expected=flags<32&&(!(flags&4)||!(flags&3));assert(portable_radio_decode(bytes,4,&out)==expected);}
 puts("Quick radio policy: atomic desired state, off cleanup, airplane restoration, individual override, sleep/resume, persistence, defaults and256 malformed states passed");
}
