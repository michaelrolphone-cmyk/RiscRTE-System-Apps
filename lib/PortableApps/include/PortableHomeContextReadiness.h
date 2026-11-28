#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "RiscResidentContextsV1.h"
#include "PortableHomePower.h"
/* A copied availability view, never a hardware/provider probe or a setting.
 * Unavailable is not false: caller must preserve the saved value verbatim. */
typedef struct {
 unsigned power_modes;
 bool face,sleep_settings,wifi,bluetooth,brightness,volume,dnd,sound,vibrate;
} portable_home_readiness;
/* -1 invalid query/option; 0 unavailable; 1 applicable. */
static inline int portable_home_readiness_query(const portable_home_readiness*s,uint32_t reason) {
 if(!s)return 0;
 unsigned field,option=0;bool options=false;
 if(reason>=RISC_CONTEXTS_SETTING_QUERY(0)&&reason<RISC_CONTEXTS_SETTING_QUERY(RISC_CONTEXTS_SETTING_COUNT))
  field=reason-RISC_CONTEXTS_SETTING_QUERY(0);
 else if(reason>=RISC_CONTEXTS_SETTING_OPTION_QUERY(0,0)&&reason<RISC_CONTEXTS_SETTING_OPTION_QUERY(RISC_CONTEXTS_SETTING_COUNT,0)){
  unsigned encoded=reason-RISC_CONTEXTS_SETTING_OPTION_QUERY(0,0);field=encoded/16;option=encoded%16;options=true;
 }else return -1;
 if(options){
  if(field==RISC_CONTEXTS_POWER){
   if(option>2)return -1;
   unsigned bit=option==0?PORTABLE_HOME_MODE_HYBRID:option==1?PORTABLE_HOME_MODE_LIGHT:PORTABLE_HOME_MODE_DEEP;
   return !!(s->power_modes&bit);
  }
  if(field==RISC_CONTEXTS_NOTIFY){
   if(option>3)return -1;
   return s->dnd&&(option==0||(option==1?s->vibrate:option==2?s->sound:s->vibrate&&s->sound));
  }
  return -1;
 }
 switch(field){
 case RISC_CONTEXTS_FACE:return s->face;
 case RISC_CONTEXTS_POWER:return s->power_modes!=0;
 case RISC_CONTEXTS_SLEEP:return s->sleep_settings&&!!(s->power_modes&(PORTABLE_HOME_MODE_LIGHT|PORTABLE_HOME_MODE_HYBRID));
 case RISC_CONTEXTS_DEEP:return s->sleep_settings&&!!(s->power_modes&(PORTABLE_HOME_MODE_DEEP|PORTABLE_HOME_MODE_HYBRID));
 case RISC_CONTEXTS_WIFI:return s->wifi;
 case RISC_CONTEXTS_BLUETOOTH:return s->bluetooth;
 case RISC_CONTEXTS_BRIGHTNESS:return s->brightness;
 case RISC_CONTEXTS_VOLUME:return s->volume;
 case RISC_CONTEXTS_NOTIFY:case RISC_CONTEXTS_DND:return s->dnd;
 default:return -1;
 }
}
