#pragma once
#include "RiscHomePresentationV1.h"
#include "RiscKeyValueV1.h"
#include <stdbool.h>
#define PORTABLE_HOME_VIEW_KEY "home_view"
#define PORTABLE_HOME_VIEW_INSTANCE 1u
enum { PORTABLE_HOME_VIEW_LOADED=0,PORTABLE_HOME_VIEW_MISSING=1,
 PORTABLE_HOME_VIEW_INVALID=-1,PORTABLE_HOME_VIEW_UNAVAILABLE=-2 };
static inline bool portable_home_view_encode(unsigned view,uint8_t out[4]){
 if(!out||view<RISC_HOME_VIEW_FACES||view>RISC_HOME_VIEW_OVERVIEW)return false;
 out[0]='H';out[1]=1;out[2]=(uint8_t)view;out[3]=(uint8_t)(view^0xa5u);return true;
}
/* Reads only: missing is a virtual default. Invalid/unavailable records are
 * never overwritten or silently repaired. Persistence uses existing atomic
 * preference transactions at an explicit user action, not this selector. */
static inline int portable_home_view_load(const risc_key_value_v1*kv,unsigned*out){
 if(!out||!kv||kv->api_version!=1||kv->struct_size<sizeof(*kv)||!kv->get)return PORTABLE_HOME_VIEW_UNAVAILABLE;
 uint8_t bytes[4];uint32_t count=0;
 int rc=kv->get(kv->context,PORTABLE_HOME_VIEW_KEY,bytes,sizeof(bytes),&count);
 if(rc==RISC_KEY_VALUE_NOT_FOUND)return PORTABLE_HOME_VIEW_MISSING;
 if(rc!=RISC_KEY_VALUE_OK)return PORTABLE_HOME_VIEW_UNAVAILABLE;
 if(count!=4||bytes[0]!='H'||bytes[1]!=1||bytes[2]<RISC_HOME_VIEW_FACES||
  bytes[2]>RISC_HOME_VIEW_OVERVIEW||bytes[3]!=(uint8_t)(bytes[2]^0xa5u))return PORTABLE_HOME_VIEW_INVALID;
 *out=bytes[2];return PORTABLE_HOME_VIEW_LOADED;
}
static inline unsigned portable_home_view_choose(const risc_home_presentation_v1*policy,int state,unsigned saved){
 if(!policy)return 0;
 if(state==PORTABLE_HOME_VIEW_LOADED)return saved>=RISC_HOME_VIEW_FACES&&saved<=RISC_HOME_VIEW_OVERVIEW?saved:0;
 return policy->default_view;
}
