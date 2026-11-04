#include "PortableSettingsModel.h"
/* The canonical controller includes every known face. Availability is policy,
 * never a target-specific compilation choice. */
#ifndef PORTABLE_DESK_POINTS_FACE
#define PORTABLE_DESK_POINTS_FACE 1
#endif
#include "PortableTimeFormat.h"
#include "PortableSleepPolicy.h"
#include "PortableAlarmSettings.h"
#include "PortableDeskClockSettings.h"
#include "PortableReaderPreferences.h"
#include <stdio.h>
#include <string.h>

static void note(PortableSettingsModel *m,const char *s){snprintf(m->message,sizeof(m->message),"%s",s);}
static bool change(PortableSettingsModel *m){
 if(m->revision==UINT32_MAX){m->phase=PSM_RETAINED;note(m,"Revision exhausted");return false;}
 ++m->revision;return true;
}
static bool live(PortableSettingsModel *m){
 if(!m||m->phase==PSM_RETAINED)return false;
 if(!m->services.live||!m->services.live(m->services.context)){
  m->phase=PSM_RETAINED;m->last_result=PSM_OPERATION_RETAINED;note(m,"Custody retained");return false;
 }
 return true;
}
static bool atomic_guard(void *context){return live(context);}
static bool outcome(PortableSettingsModel *m,int rc){
 if(!live(m))return false;
 if(rc==PSM_OPERATION_RETAINED){m->phase=PSM_RETAINED;m->last_result=rc;note(m,"Custody retained");return false;}
 return true;
}
const char *psm_key(psm_feature f){
 static const char *const keys[]={NULL,"time_zone","time_format","rtc_basis","sleep_mode","alert_mode","desk_clock_face","reader_flip_ui","reader_language","desk_direction","sleep_idle","sleep_deep",NULL,NULL};
 return (unsigned)f<PSM_FEATURE_COUNT?keys[f]:NULL;
}
const char *psm_label(psm_feature f){
 static const char *const labels[]={"Set Time","Time Zone","Time Format","RTC Basis","Clock Sleep Mode","Alarm Alerts","Clock face","Flip UI 180 degrees","Language","Desk orientation","Idle sleep timer","Deep sleep timer","Settings storage","About"};
 return (unsigned)f<PSM_FEATURE_COUNT?labels[f]:"";
}
unsigned psm_choice_count(psm_feature f){
 switch(f){case PSM_FORMAT:case PSM_FLIP:case PSM_DIRECTION:return 2;
 case PSM_SLEEP:case PSM_ALERT:return 3;case PSM_FACE:return PORTABLE_DESK_FACE_COUNT;
 case PSM_LANGUAGE:return PORTABLE_READER_LANGUAGE_COUNT;default:return 0;}
}
const char *psm_choice_label(psm_feature f,unsigned v){
 static const char *const sleep[]={"Light","Deep","Light then Deep"};
 switch(f){case PSM_FORMAT:return v==0?"12-hour":v==1?"24-hour":"Invalid";
 case PSM_SLEEP:return v<3?sleep[v]:"Invalid";case PSM_ALERT:return portable_alert_name(v);
 case PSM_FACE:return v<PORTABLE_DESK_FACE_COUNT?portable_desk_face_name(v):"Invalid";
 case PSM_LANGUAGE:return v<PORTABLE_READER_LANGUAGE_COUNT?portable_reader_language_name(v):"Invalid";
 case PSM_FLIP:return v==0?"Off":v==1?"On":"Invalid";
 case PSM_DIRECTION:return v==0?"Left":v==1?"Right":"Invalid";default:return "";}
}
psm_policy psm_default_policy(void){
 return (psm_policy){PSM_ALL_FEATURES,PSM_ALL_FEATURES&~(PSM_FEATURE(PSM_BASIS)|PSM_FEATURE(PSM_STORAGE)|PSM_FEATURE(PSM_ABOUT)),PORTABLE_SLEEP_MASK_ALL,PORTABLE_SLEEP_HYBRID,PORTABLE_DESK_SEGMENTS,60000u,300000u,false,false};
}
/* Every legacy codec call goes through a guard, including its readback. This
 * prevents a helper from issuing a second call after native identity loss. */
static bool known_kv(int rc){return rc==RISC_KEY_VALUE_OK||rc==RISC_KEY_VALUE_NOT_FOUND||rc==RISC_KEY_VALUE_BUFFER_SMALL||rc==RISC_KEY_VALUE_INVALID||rc==RISC_KEY_VALUE_CONTEXT||rc==RISC_KEY_VALUE_IO||rc==RISC_KEY_VALUE_BUSY;}
static int32_t guarded_get(void *ctx,const char *key,void *out,uint32_t cap,uint32_t *size){
 PortableSettingsModel *m=ctx;*size=0;
 if(!live(m)||!m->store||!m->store->get)return RISC_KEY_VALUE_CONTEXT;
 int rc=m->store->get(m->store->context,key,out,cap,size);
 if(!live(m)||rc==RISC_KEY_VALUE_CONTEXT||!known_kv(rc)){m->phase=PSM_RETAINED;return RISC_KEY_VALUE_CONTEXT;}
 return rc;
}
static int32_t guarded_put(void *ctx,const char *key,const void *data,uint32_t n){
 PortableSettingsModel *m=ctx;
 if(!live(m)||!m->store||!m->store->put)return RISC_KEY_VALUE_CONTEXT;
 int rc=m->store->put(m->store->context,key,data,n);
 if(!live(m)||rc==RISC_KEY_VALUE_CONTEXT||!known_kv(rc)){m->phase=PSM_RETAINED;return RISC_KEY_VALUE_CONTEXT;}
 return rc;
}
static risc_key_value_v1 guarded_store(PortableSettingsModel *m){
 return (risc_key_value_v1){1,sizeof(risc_key_value_v1),m,guarded_get,guarded_put};
}
static int32_t captured_get(void *ctx,const char *key,void *out,uint32_t cap,uint32_t *size){
 portable_preference_atomic *a=ctx;*size=0;
 if(strcmp(key,a->key))return RISC_KEY_VALUE_INVALID;
 if(!a->present)return RISC_KEY_VALUE_NOT_FOUND;
 *size=a->expected_size;if(cap<*size)return RISC_KEY_VALUE_BUFFER_SMALL;
 memcpy(out,a->expected,*size);return RISC_KEY_VALUE_OK;
}
static int32_t forbidden_put(void *ctx,const char *key,const void *p,uint32_t n){(void)ctx;(void)key;(void)p;(void)n;return RISC_KEY_VALUE_INVALID;}
static int load(PortableSettingsModel *m,psm_feature f,const risc_key_value_v1 *kv,psm_value *v){
 unsigned n=0;int rc=PSM_UNAVAILABLE;memset(v,0,sizeof(*v));
 switch(f){
 case PSM_ZONE:return portable_timezone_preference_load(kv,v->zone);
 case PSM_FORMAT:rc=portable_time_format_load(kv,&n);break;
 case PSM_BASIS:return portable_rtc_basis_load(kv,&v->basis);
 case PSM_SLEEP:rc=portable_sleep_load_profile(kv,m->policy.sleep_modes,m->policy.sleep_default,&n);break;
 case PSM_ALERT:rc=portable_alert_load(kv,&n);break;
 case PSM_FACE:rc=portable_desk_face_load(kv,&n);if(rc!=PSM_LOADED)n=m->policy.face_default;break;
 case PSM_FLIP:case PSM_LANGUAGE:rc=portable_reader_preference_load(kv,f==PSM_LANGUAGE,&n);break;
 case PSM_DIRECTION:rc=portable_desk_direction_load(kv,&n);break;
 case PSM_IDLE_TIMER:case PSM_DEEP_TIMER:
  rc=portable_sleep_timer_load(kv,f==PSM_DEEP_TIMER,&v->number);
  if(rc!=PSM_LOADED)v->number=f==PSM_DEEP_TIMER?m->policy.deep_default_ms:m->policy.idle_default_ms;
  return rc;
 default:return rc;
 }
 v->number=n;return rc;
}
static bool availability(PortableSettingsModel *m){
 if(!live(m))return false;
 if(m->services.storage_state){unsigned state=PSM_STORAGE_UNAVAILABLE;
  int rc=m->services.storage_state(m->services.context,&state);if(!outcome(m,rc))return false;
  m->storage_state=rc==PSM_OK&&state<=PSM_STORAGE_CONFLICT?state:PSM_STORAGE_UNAVAILABLE;
 }
 return true;
}
static bool writable(PortableSettingsModel *m){
 return (m->policy.writable&PSM_FEATURE(m->editing))&&
  !(m->editing==PSM_SLEEP&&m->policy.desk_lock)&&!(m->editing==PSM_ALERT&&m->policy.visual_alerts);
}
bool psm_refresh(PortableSettingsModel *m){
 if(!m||m->route!=PSM_ROOT||m->phase!=PSM_IDLE||!availability(m))return false;
 risc_key_value_v1 g=guarded_store(m);const risc_key_value_v1 *kv=m->store?&g:NULL;
 for(unsigned f=0;f<PSM_FEATURE_COUNT;++f){
  bool dependency=(m->policy.features&PSM_FEATURE(PSM_TIME))&&(f==PSM_BASIS||f==PSM_ZONE);
  if((!(m->policy.features&PSM_FEATURE(f))&&!dependency)||!psm_key((psm_feature)f))continue;
  psm_value v;int rc;
  if(m->storage_state!=PSM_STORAGE_AVAILABLE){rc=load(m,(psm_feature)f,NULL,&v);}
  else if(m->atomic_store){
   portable_preference_atomic snapshot={0};
   rc=portable_preference_atomic_begin(&snapshot,m->atomic_store,psm_key((psm_feature)f),PORTABLE_PREFERENCE_USER,atomic_guard,m);
   if(rc==RISC_ATOMIC_RETAINED){m->phase=PSM_RETAINED;return false;}
   if(rc==PORTABLE_PREFERENCE_READY){risc_key_value_v1 captured={1,sizeof(captured),&snapshot,captured_get,forbidden_put};rc=load(m,(psm_feature)f,&captured,&v);}
   else {memset(&v,0,sizeof(v));rc=PSM_UNAVAILABLE;}
  }else rc=load(m,(psm_feature)f,kv,&v);
  if(!live(m))return false;
  /* Unavailable refresh cannot replace the last confirmed value. */
  if(rc!=PSM_UNAVAILABLE||!m->has_confirmed[f])m->confirmed[f]=v;
  if(rc==PSM_LOADED)m->has_confirmed[f]=true;
  if(m->status[f]!=PSM_UNCONFIRMED)m->status[f]=rc;
 }
 return change(m);
}
bool psm_init(PortableSettingsModel *m,const psm_policy *p,const psm_services *s,const risc_key_value_v1 *kv,const risc_key_value_atomic_v1 *atomic){
 if(!m||!p||!s||!s->live||p->features&~PSM_ALL_FEATURES||p->writable&~p->features||
  p->sleep_modes&~7u||!portable_sleep_supported(p->sleep_modes,p->sleep_default)||p->face_default>=PORTABLE_DESK_FACE_COUNT||
  p->idle_default_ms<5000||p->idle_default_ms>3600000||p->idle_default_ms%1000||
  p->deep_default_ms<60000||p->deep_default_ms>3600000||p->deep_default_ms%1000)return false;
 if(kv&&(kv->api_version!=1||kv->struct_size<sizeof(*kv)||!kv->get||!kv->put))return false;
 if(atomic&&!portable_preference_atomic_api_valid(atomic))return false;
 memset(m,0,sizeof(*m));m->policy=*p;m->services=*s;m->store=kv;m->atomic_store=atomic;
 m->route=PSM_ROOT;m->revision=1;m->editing=PSM_FEATURE_COUNT;m->fold_choice=-1;
 m->storage_state=kv||atomic?PSM_STORAGE_AVAILABLE:PSM_STORAGE_UNAVAILABLE;
 for(unsigned i=0;i<PSM_FEATURE_COUNT;++i)m->status[i]=PSM_UNAVAILABLE;
 return psm_refresh(m);
}
bool psm_open(PortableSettingsModel *m,psm_feature f){
 if(!m||(unsigned)f>=PSM_FEATURE_COUNT||m->phase!=PSM_IDLE||m->route!=PSM_ROOT||!(m->policy.features&PSM_FEATURE(f))||!live(m))return false;
 if(!psm_refresh(m))return false;
 m->editing=f;m->dirty=false;m->apply_unconfirmed=false;m->cancel_requested=false;m->operation_started=false;m->fold_choice=-1;m->last_result=PSM_OK;note(m,"");
 if(f==PSM_STORAGE||f==PSM_ABOUT){m->route=f==PSM_STORAGE?PSM_STORAGE_ROUTE:PSM_ABOUT_ROUTE;return change(m);}
 m->draft=m->confirmed[f];m->uncertain=m->status[f]==PSM_UNCONFIRMED;
 if(!availability(m))return false;
 if(f==PSM_TIME){
  /* Clock operation owns its authority/metadata precondition through cancel. */
  if(!m->services.clock_read||!m->services.clock_start||!m->services.clock_poll||!m->services.clock_cancel){note(m,"Clock control unavailable");return false;}
  if(m->services.clock_begin){int rc=m->services.clock_begin(m->services.context);if(!outcome(m,rc==PSM_PENDING?PSM_OPERATION_RETAINED:rc)||rc!=PSM_OK)return false;}
  m->clock_active=true;memset(&m->clock_result,0,sizeof(m->clock_result));
  int rc=m->services.clock_read(m->services.context,&m->draft.civil);if(!outcome(m,rc==PSM_PENDING?PSM_OPERATION_RETAINED:rc))return false;
  if(rc!=PSM_OK){note(m,"Clock unavailable");psm_cancel(m);return false;}
 }else if(m->atomic_store&&psm_key(f)&&writable(m)&&m->storage_state==PSM_STORAGE_AVAILABLE){
  int rc=portable_preference_atomic_begin(&m->atomic,m->atomic_store,psm_key(f),PORTABLE_PREFERENCE_USER,atomic_guard,m);
  if(rc==RISC_ATOMIC_RETAINED){m->phase=PSM_RETAINED;return false;}
  if(rc!=PORTABLE_PREFERENCE_READY){m->last_result=rc;note(m,"Preference unavailable or changed; reload");return false;}
  risc_key_value_v1 snapshot={1,sizeof(snapshot),&m->atomic,captured_get,forbidden_put};
  int status=load(m,f,&snapshot,&m->draft);m->status[f]=status;
 }
 m->draft_valid=f!=PSM_ALERT||m->draft.number>=1u;
 m->route=PSM_EDITOR;return change(m);
}
bool psm_set_number(PortableSettingsModel *m,uint32_t n){
 if(!m||m->phase!=PSM_IDLE||m->route!=PSM_EDITOR||!live(m)||!writable(m))return false;
 switch(m->editing){
 case PSM_SLEEP:if(!portable_sleep_supported(m->policy.sleep_modes,n))return false;break;
 case PSM_ALERT:if(n<1||n>3)return false;break;
 case PSM_IDLE_TIMER:case PSM_DEEP_TIMER:if(n<(m->editing==PSM_DEEP_TIMER?60000u:5000u)||n>3600000u||n%1000u)return false;break;
 default:{unsigned count=psm_choice_count(m->editing);if(!count||n>=count)return false;break;}
 }
 m->draft.number=n;m->draft_valid=true;m->dirty=true;return change(m);
}
bool psm_set_zone(PortableSettingsModel *m,const char *id,size_t cap){
 if(!m||m->phase!=PSM_IDLE||m->route!=PSM_EDITOR||m->editing!=PSM_ZONE||!live(m)||!writable(m))return false;
 int index=portable_timezone_find(id,cap);if(index<0)return false;
 memset(m->draft.zone,0,sizeof(m->draft.zone));strcpy(m->draft.zone,portable_timezone_get((unsigned)index)->id);
 m->draft_valid=m->dirty=true;return change(m);
}
bool psm_set_civil(PortableSettingsModel *m,const portable_timezone_civil *v){
 if(!m||!v||m->phase!=PSM_IDLE||m->route!=PSM_EDITOR||m->editing!=PSM_TIME||!live(m)||!writable(m))return false;
 portable_timezone_rule utc={0};portable_timezone_inverse inverse;
 if(v->year<2000||v->year>2099||portable_timezone_local_to_utc(&utc,v,&inverse)!=PORTABLE_TIMEZONE_OK)return false;
 m->draft.civil=*v;m->draft_valid=m->dirty=true;m->fold_choice=-1;return change(m);
}
bool psm_choose_fold(PortableSettingsModel *m,unsigned fold){
 if(!m||m->route!=PSM_FOLD||m->phase!=PSM_IDLE||fold>1||!live(m))return false;
 m->fold_choice=(int)fold;m->route=PSM_EDITOR;return change(m);
}
static bool prepare_clock(PortableSettingsModel *m){
 if(m->status[PSM_BASIS]>PSM_MISSING||m->status[PSM_ZONE]>PSM_MISSING){note(m,"Choose a valid timezone and RTC basis first");return false;}
 portable_set_time_request r={.local=m->draft.civil,.basis=m->confirmed[PSM_BASIS].basis,.fold_choice=m->fold_choice,.native_readback_budget_us=1000000};
 if(portable_timezone_resolve(m->confirmed[PSM_ZONE].zone,sizeof(m->confirmed[PSM_ZONE].zone),&r.rule)!=PORTABLE_TIMEZONE_OK){note(m,"Time Zone unavailable");return false;}
 portable_set_time_plan plan;int rc=portable_set_time_prepare(&r,&plan);
 if(rc==PORTABLE_REALTIME_FOLD){m->route=PSM_FOLD;note(m,"Choose first or second occurrence");(void)change(m);return false;}
 if(rc!=PORTABLE_REALTIME_OK){note(m,rc==PORTABLE_REALTIME_GAP?"DST gap - time does not exist":"Invalid date or clock range");return false;}
 m->clock_request=r;return true;
}
bool psm_save(PortableSettingsModel *m){
 if(!m||m->phase!=PSM_IDLE||m->route!=PSM_EDITOR||!live(m)||!writable(m)||!m->draft_valid||!availability(m))return false;
 if(m->storage_state!=PSM_STORAGE_AVAILABLE){note(m,"Storage unavailable; draft kept");return false;}
 if(m->editing==PSM_TIME){
  if(!m->clock_active){note(m,"Reload clock authority before retry; draft kept");return false;}
  if(!prepare_clock(m))return false;
 }
 if(m->atomic_store&&m->editing!=PSM_TIME&&(!m->atomic.valid||m->atomic.submitted)){note(m,"Reload before a new save attempt; draft kept");return false;}
 m->phase=PSM_WAIT_PRESENTATION;note(m,"Saving - not confirmed");if(!change(m))return false;
 m->save_revision=m->revision;return true;
}
bool psm_presented(PortableSettingsModel *m,uint32_t revision){
 if(!m||m->phase!=PSM_WAIT_PRESENTATION||revision!=m->save_revision||!live(m))return false;
 m->phase=PSM_COMMITTING;return true;
}
static uint32_t encode(PortableSettingsModel *m,uint8_t bytes[64]){
 memset(bytes,0,64);unsigned n=m->draft.number;
 switch(m->editing){
 case PSM_ALERT:bytes[0]=(uint8_t)n;return 1;
 case PSM_ZONE:
  bytes[0]='T';bytes[1]='Z';bytes[2]=1;memcpy(bytes+4,m->draft.zone,sizeof(m->draft.zone));bytes[3]=0xa5;
  for(unsigned i=0;i<44;++i)if(i!=3)bytes[3]^=bytes[i];
  return 44;
 case PSM_IDLE_TIMER:case PSM_DEEP_TIMER:n/=1000;bytes[0]=0x54;bytes[1]=1;bytes[2]=(uint8_t)n;bytes[3]=(uint8_t)(n>>8);bytes[4]=bytes[2]^bytes[3]^0xa5;return 5;
 default:break;
 }
 switch(m->editing){case PSM_FORMAT:bytes[0]=0x54;break;case PSM_SLEEP:bytes[0]=0x53;break;
 case PSM_FACE:bytes[0]=0x46;break;case PSM_FLIP:bytes[0]=0x52;break;case PSM_LANGUAGE:bytes[0]=0x4c;break;case PSM_DIRECTION:bytes[0]=0x44;break;default:return 0;}
 bytes[1]=1;bytes[2]=(uint8_t)n;bytes[3]=(uint8_t)n^0xa5;return 4;
}
static bool legacy_save(PortableSettingsModel *m){
 risc_key_value_v1 g=guarded_store(m);const risc_key_value_v1 *kv=m->store?&g:NULL;unsigned n=m->draft.number;
 switch(m->editing){case PSM_ZONE:return portable_timezone_preference_save(kv,m->draft.zone,sizeof(m->draft.zone))>=0;
 case PSM_FORMAT:return portable_time_format_save(kv,n);case PSM_SLEEP:return portable_sleep_save_profile(kv,m->policy.sleep_modes,n);
 case PSM_ALERT:return portable_alert_save(kv,n);case PSM_FACE:return portable_desk_face_save(kv,n);
 case PSM_FLIP:case PSM_LANGUAGE:return portable_reader_preference_save(kv,m->editing==PSM_LANGUAGE,n);
 case PSM_DIRECTION:return portable_desk_direction_save(kv,n);
 case PSM_IDLE_TIMER:case PSM_DEEP_TIMER:return portable_sleep_timer_save(kv,m->editing==PSM_DEEP_TIMER,n);default:return false;}
}
static void finished(PortableSettingsModel *m,int rc,bool atomic){
 if(!outcome(m,rc))return;
 m->phase=PSM_IDLE;m->last_result=rc;
 if(rc==PSM_OK){
  m->confirmed[m->editing]=m->draft;m->has_confirmed[m->editing]=true;m->status[m->editing]=PSM_LOADED;
  if(!atomic&&m->editing!=PSM_TIME){
   risc_key_value_v1 g=guarded_store(m);psm_value checked;int state=load(m,m->editing,m->store?&g:NULL,&checked);
   if(!live(m))return;
   m->status[m->editing]=state;m->has_confirmed[m->editing]=state==PSM_LOADED;
  }
  if(m->editing==PSM_TIME){
   if(m->clock_result.metadata_outcome==PORTABLE_SET_TIME_CONFIRMED){m->confirmed[PSM_BASIS].basis=m->clock_result.plan.basis;m->status[PSM_BASIS]=PSM_LOADED;m->has_confirmed[PSM_BASIS]=true;}
   else if(m->clock_result.metadata_outcome==PORTABLE_SET_TIME_UNCONFIRMED)m->status[PSM_BASIS]=PSM_UNCONFIRMED;
  }
  m->dirty=m->uncertain=false;note(m,atomic?"Saved locally; SD backup not confirmed":m->status[m->editing]==PSM_MISSING?"Default unchanged; no record written":m->status[m->editing]==PSM_INVALID?"Default unchanged; stored record remains invalid":"Saved");
  if(m->services.applied&&m->editing!=PSM_TIME){int applied=m->services.applied(m->services.context,m->editing,&m->draft);if(!outcome(m,applied==PSM_PENDING?PSM_OPERATION_RETAINED:applied))return;
   if(applied!=PSM_OK){m->apply_unconfirmed=true;note(m,"Preference saved; live apply unconfirmed");}}
  m->route=PSM_ROOT;
 }else{m->uncertain=true;m->status[m->editing]=PSM_UNCONFIRMED;note(m,rc==PSM_CANCELLED?"Save cancelled; draft kept":"Save unconfirmed; draft kept");}
 (void)change(m);
}
void psm_tick(PortableSettingsModel *m){
 if(!m||(m->phase!=PSM_COMMITTING&&m->phase!=PSM_CANCELLING)||!live(m))return;
 bool cancel=m->phase==PSM_CANCELLING;
 if(m->editing==PSM_TIME){
  int rc;
  if(cancel)rc=m->services.clock_cancel(m->services.context,&m->clock_result);
  else if(!m->operation_started){rc=m->services.clock_start(m->services.context,&m->clock_request,&m->clock_result);if(rc!=PSM_BUSY)m->operation_started=true;}
  else rc=m->services.clock_poll(m->services.context,&m->clock_result);
  if(!outcome(m,rc)||rc==PSM_PENDING||rc==PSM_BUSY)return;
  m->clock_active=false;
  if(cancel&&!m->operation_started&&rc==PSM_CANCELLED){m->phase=PSM_IDLE;m->route=PSM_ROOT;m->dirty=m->uncertain=false;note(m,"Cancelled");(void)change(m);return;}
  finished(m,rc,false);return;
 }
 if(m->atomic_store){
  int rc;
  if(cancel)rc=portable_preference_atomic_cancel(&m->atomic);
  else if(m->atomic.token)rc=portable_preference_atomic_collect(&m->atomic);
  else{if(!m->atomic.desired_size){uint8_t bytes[64];uint32_t n=encode(m,bytes);rc=portable_preference_atomic_prepare(&m->atomic,bytes,n);if(rc!=PORTABLE_PREFERENCE_READY){finished(m,PSM_ERROR,true);return;}}
   rc=portable_preference_atomic_submit(&m->atomic);}
  if(!live(m))return;
  if(rc==RISC_ATOMIC_RETAINED){m->phase=PSM_RETAINED;note(m,"Custody retained");return;}
  if(rc==RISC_ATOMIC_PENDING||rc==PORTABLE_PREFERENCE_READY)return;
  finished(m,rc==RISC_ATOMIC_OK?PSM_OK:rc==RISC_ATOMIC_CANCELLED?PSM_CANCELLED:PSM_ERROR,true);m->last_result=rc;
  if(m->phase!=PSM_RETAINED&&(rc==RISC_ATOMIC_STALE||rc==RISC_ATOMIC_CONFLICT)){
   m->status[m->editing]=PSM_CONFLICT;note(m,"Preference changed; reload before saving draft");
  }
  return;
 }
 if(cancel){finished(m,PSM_CANCELLED,false);return;}
 bool saved=legacy_save(m);if(!live(m))return;finished(m,saved?PSM_OK:PSM_ERROR,false);
}
void psm_cancel(PortableSettingsModel *m){
 if(!m||!live(m))return;
 if(m->phase==PSM_COMMITTING||m->phase==PSM_CANCELLING||m->atomic.token||m->clock_active){m->phase=PSM_CANCELLING;note(m,"Cancelling; waiting for custody");(void)change(m);return;}
 if(m->phase==PSM_WAIT_PRESENTATION)m->phase=PSM_IDLE;
 if(m->uncertain){note(m,"Unconfirmed draft kept; discard explicitly");(void)change(m);return;}
 m->dirty=false;m->route=PSM_ROOT;m->atomic.valid=false;note(m,"Cancelled");(void)change(m);
}
bool psm_discard(PortableSettingsModel *m){
 if(!m||m->phase!=PSM_IDLE||m->atomic.token||m->clock_active||!live(m))return false;
 m->dirty=m->uncertain=false;m->atomic.valid=false;m->route=PSM_ROOT;note(m,"Draft discarded; persisted values will be reloaded");
 /* Discard is not rollback and does not claim an uncertain write was undone. */
 if((unsigned)m->editing<PSM_FEATURE_COUNT)m->status[m->editing]=PSM_UNAVAILABLE;
 return psm_refresh(m);
}
bool psm_can_handoff(const PortableSettingsModel *m){return m&&m->phase==PSM_IDLE&&!m->atomic.token&&!m->clock_active;}

bool psm_reload_draft(PortableSettingsModel *m){
 if(!m||m->phase!=PSM_IDLE||m->route!=PSM_EDITOR||m->atomic.token||m->clock_active||!writable(m)||!availability(m))return false;
 if(m->storage_state!=PSM_STORAGE_AVAILABLE){note(m,"Storage not ready; draft kept");return false;}
 if(m->editing==PSM_TIME){
  if(!m->services.clock_begin){note(m,"Clock authority reload unavailable");return false;}
  int rc=m->services.clock_begin(m->services.context);if(!outcome(m,rc==PSM_PENDING?PSM_OPERATION_RETAINED:rc)||rc!=PSM_OK)return false;
  m->clock_active=true;m->operation_started=false;
  /* Refresh read-only metadata at this explicit retry boundary; draft stays. */
  uint32_t route=m->route;m->route=PSM_ROOT;bool refreshed=psm_refresh(m);m->route=route;
  if(!refreshed)return false;
  memset(&m->clock_result,0,sizeof(m->clock_result));
 }else if(m->atomic_store){
  int rc=portable_preference_atomic_begin(&m->atomic,m->atomic_store,psm_key(m->editing),PORTABLE_PREFERENCE_USER,atomic_guard,m);
  if(rc==RISC_ATOMIC_RETAINED){m->phase=PSM_RETAINED;return false;}
  m->last_result=rc;
  if(rc!=PORTABLE_PREFERENCE_READY){note(m,"Reload failed; draft kept");return false;}
 }
 note(m,"Preconditions reloaded; draft kept. Save explicitly");return change(m);
}
