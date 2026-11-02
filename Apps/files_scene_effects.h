#pragma once
#include "RiscResidentSettingsEffectsV1.h"
#include <stdbool.h>
#include <string.h>
/* One settled, serialized effect attempt. Persistence is already committed.
 * This controller never retries APPLY after OK and never writes preferences. */
typedef struct {
 bool (*alive)(void*);void (*retain)(void*);int (*settle)(void*);
 int (*call)(void*,uint32_t);bool (*done)(void*,unsigned,unsigned);void *context;
 unsigned effect,phase,result;bool retained,busy,unsupported;
} files_effects;
enum {FE_NONE,FE_QUERY,FE_APPLY,FE_RESULT,FE_PROGRESS,FE_DONE};
static bool files_effects_fail(files_effects*s){
 if(!s->retained){s->retained=true;if(s->retain)s->retain(s->context);}return false;
}
static bool files_effects_alive(files_effects*s){
 return !s->retained&&s->alive&&s->alive(s->context);
}
static void files_effects_init(files_effects*s,bool(*alive)(void*),void(*retain)(void*),
 int(*settle)(void*),int(*call)(void*,uint32_t),bool(*done)(void*,unsigned,unsigned),void*c){
 *s=(files_effects){.alive=alive,.retain=retain,.settle=settle,.call=call,.done=done,.context=c};
}
/* -1 retained, 0 pending, 1 terminal outcome copied, 2 clean unsupported.
 * Caller keeps pending bits when unsupported and discloses missing host support.
 * Inhibition preserves exact state across sharing, presenting and held input. */
static int files_effects_step(files_effects*s,uint32_t pending,bool ready){
 if(!files_effects_alive(s)){files_effects_fail(s);return -1;}
 if(s->busy||pending&~UINT32_C(0x1e)){files_effects_fail(s);return -1;}
 if(!s->phase){
  s->unsupported=false;
  if(!pending)return 0;
  for(s->effect=1;s->effect<=4;s->effect++)if(pending&(1u<<s->effect))break;
  s->phase=FE_QUERY;
 }
 if(!(pending&(1u<<s->effect))){files_effects_fail(s);return -1;}
 if(!ready)return 0;
 s->busy=true;int settled=s->settle(s->context);s->busy=false;
 if(!files_effects_alive(s)){files_effects_fail(s);return -1;}
 if(!settled)return 0;
 if(settled!=1){files_effects_fail(s);return -1;}
 if(s->phase==FE_DONE){
  s->busy=true;bool ok=s->done(s->context,s->effect,s->result);s->busy=false;
  if(!files_effects_alive(s)||!ok){files_effects_fail(s);return -1;}
  if(s->result==RISC_SETTINGS_PENDING){s->phase=FE_PROGRESS;return 0;}
  s->phase=FE_NONE;return 1;
 }
 uint32_t reason=s->phase==FE_QUERY?RISC_SETTINGS_QUERY(s->effect):
  s->phase==FE_APPLY?RISC_SETTINGS_APPLY(s->effect):
  s->phase==FE_PROGRESS?RISC_SETTINGS_PROGRESS(s->effect):RISC_SETTINGS_RESULT(s->effect,s->result);
 s->busy=true;int rc=s->call(s->context,reason);s->busy=false;
 if(!files_effects_alive(s)){files_effects_fail(s);return -1;}
 if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_BUSY){files_effects_fail(s);return -1;}
 if(s->phase==FE_QUERY){
  if(rc==RISC_RESIDENT_BUSY){s->unsupported=true;s->phase=FE_NONE;return 2;}
  s->phase=FE_APPLY;return 0;
 }
 if(s->phase==FE_APPLY||s->phase==FE_PROGRESS){
  if(rc==RISC_RESIDENT_BUSY){
   /* PROGRESS BUSY contradicts the copied pending result; never replay APPLY. */
   if(s->phase==FE_PROGRESS){files_effects_fail(s);return -1;}
   return 0;
  }
  s->phase=FE_RESULT;s->result=RISC_SETTINGS_APPLIED;return 0;
 }
 if(rc==RISC_RESIDENT_OK){s->phase=FE_DONE;return 0;}
 if(++s->result>RISC_SETTINGS_PENDING){files_effects_fail(s);return -1;}
 return 0;
}
