/* Explicit Quick controller. Capture before the modal draft is loaded,
 * including taps whose preview/release are coalesced. Retain it through Save,
 * never recapture on stale/busy or infer an intent from an automatic KV put. */
#include "PortablePreferenceAtomic.h"
#include "PortableContextPreferences.h"
static risc_runtime_capability_v1 quick_atomic_grant;
static portable_preference_atomic quick_atomic_edit;
static char quick_atomic_key[16];
static bool quick_atomic_attempted;
static uint64_t quick_atomic_revision;
static unsigned quick_atomic_captured_contact;
/* Ordered input can run while paper presentation is pending. Capture each
 * reducer result before its bit mask is ORed with a later user action. */
#define QUICK_ATOMIC_QUEUE_CAPACITY 32u
typedef struct {
 uint32_t actions;unsigned contact;
 uint8_t brightness,tone,volume;bool torch,action_torch,dnd,contexts;
} quick_atomic_action;
static quick_atomic_action quick_atomic_queue[QUICK_ATOMIC_QUEUE_CAPACITY];
static unsigned quick_atomic_queue_head,quick_atomic_queue_count,quick_atomic_delivered_contact;
static bool quick_atomic_queue_overflow,quick_atomic_delivering;
static void quick_atomic_discard(void) {
 quick_atomic_queue_head=quick_atomic_queue_count=0;quick_atomic_queue_overflow=quick_atomic_delivering=false;
}
static void quick_atomic_enqueue(void) {
 uint32_t actions=pqa_take_action(&quick.ui);if(!actions)return;
 if(quick_atomic_queue_overflow){quick.ui.error_flags|=PQA_ERROR_SAVE;return;}
 quick_atomic_action next={actions,quick_atomic_contact,quick.ui.action_brightness,
#ifdef PORTABLE_FRONTLIGHT_TONE
  quick.ui.action_tone,
#else
  0,
#endif
  quick.ui.action_volume,quick.ui.torch,quick.ui.action_torch,quick.ui.action_dnd,quick.ui.action_contexts};
 /* Consecutive previews of the same contact may collapse; commits and
  * distinct controls never do. There is no provider call at this boundary. */
 const uint32_t previews=PQA_BRIGHTNESS_PREVIEW|PQA_TONE_PREVIEW;
 if(!(actions&~previews)&&quick_atomic_queue_count) {
  unsigned last=(quick_atomic_queue_head+quick_atomic_queue_count-1)%QUICK_ATOMIC_QUEUE_CAPACITY;
  if(quick_atomic_queue[last].actions==actions&&quick_atomic_queue[last].contact==next.contact){quick_atomic_queue[last]=next;return;}
 }
 if(quick_atomic_queue_count==QUICK_ATOMIC_QUEUE_CAPACITY) {
  quick_atomic_queue_overflow=true;quick.ui.error_flags|=PQA_ERROR_SAVE;
  pqa_cancel_input(&quick.ui);(void)pqa_take_action(&quick.ui);return;
 }
 quick_atomic_queue[(quick_atomic_queue_head+quick_atomic_queue_count++)%QUICK_ATOMIC_QUEUE_CAPACITY]=next;
}
static void quick_atomic_show_queue_error(void) {if(quick_atomic_queue_overflow)quick.ui.error_flags|=PQA_ERROR_SAVE;}
static uint32_t quick_atomic_take_action(void) {
 quick_atomic_enqueue();if(quick_atomic_queue_overflow)quick.ui.error_flags|=PQA_ERROR_SAVE;
 if(!quick_atomic_queue_count)return 0;
 const quick_atomic_action *a=&quick_atomic_queue[quick_atomic_queue_head];
 quick.ui.action_brightness=a->brightness;
#ifdef PORTABLE_FRONTLIGHT_TONE
 quick.ui.action_tone=a->tone;
#endif
 quick.ui.action_volume=a->volume;
 quick.ui.torch=a->torch;quick.ui.action_torch=a->action_torch;quick.ui.action_dnd=a->dnd;quick.ui.action_contexts=a->contexts;
 quick_atomic_delivered_contact=a->contact;quick_atomic_delivering=true;uint32_t actions=a->actions;
 quick_atomic_queue_head=(quick_atomic_queue_head+1)%QUICK_ATOMIC_QUEUE_CAPACITY;--quick_atomic_queue_count;
 return actions;
}
static bool quick_atomic_guard(void *unused) {
 (void)unused;return !native_custody_retained&&!failed&&!quick.retained;
}
static bool quick_atomic_owned(void) {
 return quick_atomic_running||quick_atomic_edit.token!=0;
}
static bool quick_atomic_result(int rc) {
 if(rc==RISC_ATOMIC_RETAINED) {
  quick.retained=true;if(!native_custody_retained)portable_adapter_retain();
  return false;
 }
 return quick_atomic_guard(NULL);
}
static bool quick_atomic_close(void) {
 if(!quick_atomic_grant.api&&!quick_atomic_edit.token&&!quick_atomic_running) {
  quick_atomic_revision=0;quick_atomic_attempted=false;quick_atomic_discard();return true;
 }
 if(!quick_atomic_guard(NULL))return false;
 if(quick_atomic_edit.token) {
  int rc=portable_preference_atomic_cancel(&quick_atomic_edit);
  if(rc==RISC_ATOMIC_PENDING||rc==RISC_ATOMIC_RETAINED)return quick_atomic_result(RISC_ATOMIC_RETAINED);
 }
 if(quick_atomic_running)return quick_atomic_result(RISC_ATOMIC_RETAINED);
 if(quick_atomic_grant.api&&!rt->release(&quick_atomic_grant))return quick_atomic_result(RISC_ATOMIC_RETAINED);
 if(!quick_atomic_guard(NULL))return false;
 quick_atomic_grant=(risc_runtime_capability_v1){0};quick_atomic_edit=(portable_preference_atomic){0};
 quick_atomic_attempted=false;quick_atomic_revision=0;quick_atomic_key[0]=0;quick_atomic_discard();return true;
}
static bool quick_atomic_modal_begin(void) {
 if(!quick_atomic_close())return false;
#ifdef PORTABLE_SETTINGS_AVAILABILITY
 if(!portable_home_settings_poll())return false;
#endif
 quick_atomic_grant=(risc_runtime_capability_v1){.struct_size=sizeof(quick_atomic_grant)};
 if(!rt->acquire(PORTABLE_PREFERENCE_CAPABILITY,1,1,&quick_atomic_grant)) {
  quick.ui.error_flags|=PQA_ERROR_SAVE;return quick_atomic_guard(NULL);
 }
 if(!quick_atomic_guard(NULL))return false;
 if(quick_atomic_grant.struct_size!=sizeof(quick_atomic_grant)||!quick_atomic_grant.slot||
    !quick_atomic_grant.generation||!portable_preference_atomic_api_valid(quick_atomic_grant.api))return quick_atomic_result(RISC_ATOMIC_RETAINED);
#ifdef PORTABLE_SETTINGS_AVAILABILITY
 if(!portable_home_settings_available()){quick.ui.error_flags|=PQA_ERROR_SAVE;return true;}
#endif
 const risc_key_value_atomic_v1 *api=quick_atomic_grant.api;
 risc_key_value_atomic_state_v1 state={.struct_size=sizeof(state)};
 int rc=api->state(api->context,&state);
 if(!quick_atomic_guard(NULL))return false;
 if(!rc&&!portable_preference_atomic_state_valid(&state))rc=RISC_ATOMIC_INVALID;
 if(!rc)quick_atomic_revision=state.revision;else quick.ui.error_flags|=PQA_ERROR_SAVE;
 return quick_atomic_result(rc);
}
static const char *quick_atomic_action_key(uint32_t actions) {
 if(actions&(PQA_BRIGHTNESS_PREVIEW|PQA_BRIGHTNESS_COMMIT))return PQA_BRIGHTNESS_KEY;
#ifdef PORTABLE_FRONTLIGHT_TONE
 if(actions&(PQA_TONE_PREVIEW|PQA_TONE_COMMIT))return PQA_TONE_KEY;
#endif
 if(actions&(PQA_VOLUME_COMMIT|PQA_SILENT))return PQA_VOLUME_KEY;
 if(actions&PQA_DND)return PQA_DND_KEY;
 if(actions&PQA_CONTEXTS)return PORTABLE_CONTEXT_ENABLED_KEY;
#ifdef PORTABLE_QUICK_RADIOS
 if(actions&(PQA_WIFI|PQA_BLUETOOTH|PQA_AIRPLANE))return PORTABLE_RADIO_KEY;
#endif
 return NULL;
}
/* Reject a draft whose displayed base no longer matches the captured Core.
 * Missing records retain their virtual defaults without repair writes. */
static bool quick_atomic_matches_display(void) {
 const portable_preference_atomic *s=&quick_atomic_edit;
 if(!s->valid)return false;
 if(!strcmp(s->key,PQA_BRIGHTNESS_KEY))return s->present?
  s->expected_size==1&&s->expected[0]==quick.brightness:quick.brightness==PQA_BRIGHTNESS_DEFAULT;
#ifdef PORTABLE_FRONTLIGHT_TONE
 if(!strcmp(s->key,PQA_TONE_KEY))return s->present?
  s->expected_size==1&&s->expected[0]==quick.tone:quick.tone==PQA_TONE_DEFAULT;
#endif
 if(!strcmp(s->key,PQA_VOLUME_KEY))return s->present?
  s->expected_size==1&&s->expected[0]==quick.volume:quick.volume==PQA_VOLUME_DEFAULT;
 if(!strcmp(s->key,PQA_DND_KEY))return s->present?
  s->expected_size==1&&s->expected[0]==(uint8_t)quick.dnd_enabled:!quick.dnd_enabled;
 if(!strcmp(s->key,PORTABLE_CONTEXT_ENABLED_KEY)) {
  uint8_t e=quick.ui.contexts_enabled?1:0;
  const uint8_t b[]={'C',1,e,(uint8_t)(e^0xa5u)};
  return s->present?s->expected_size==4&&!memcmp(s->expected,b,4):!e;
 }
#ifdef PORTABLE_QUICK_RADIOS
 if(!strcmp(s->key,PORTABLE_RADIO_KEY)) {
  uint8_t flags=0;return s->present?portable_radio_decode(s->expected,s->expected_size,&flags)&&flags==quick_radios.flags:
   quick_radios.flags==PORTABLE_RADIO_WIFI;
 }
#endif
 return false;
}
static bool quick_atomic_capture(uint32_t actions) {
 const char *key=quick_atomic_action_key(actions);if(!key)return true;
 unsigned contact=quick_atomic_delivering?quick_atomic_delivered_contact:quick_atomic_contact;
 if(quick_atomic_attempted&&quick_atomic_captured_contact==contact&&!strcmp(key,quick_atomic_key))return true;
 if(quick_atomic_owned())return quick_atomic_result(RISC_ATOMIC_RETAINED);
 quick_atomic_edit=(portable_preference_atomic){0};
 quick_atomic_attempted=true;quick_atomic_captured_contact=contact;
 memcpy(quick_atomic_key,key,strlen(key)+1);
 int rc=quick_atomic_revision?portable_preference_atomic_begin_at(&quick_atomic_edit,quick_atomic_grant.api,key,
  PORTABLE_PREFERENCE_USER,quick_atomic_guard,NULL,quick_atomic_revision):RISC_ATOMIC_INVALID;
 if(rc==PORTABLE_PREFERENCE_READY&&!quick_atomic_matches_display()) {
  quick_atomic_edit.valid=false;quick_atomic_edit.status=rc=RISC_ATOMIC_STALE;
 }
 if(rc!=PORTABLE_PREFERENCE_READY)quick.ui.error_flags|=PQA_ERROR_SAVE;
 return quick_atomic_result(rc);
}
static bool quick_atomic_save(void *unused,const char *key,const void *value,uint32_t size,const uint8_t *restore) {
 (void)unused;
#ifdef PORTABLE_SETTINGS_AVAILABILITY
 if(!portable_home_settings_available())return false;
#endif
 if(!quick_atomic_guard(NULL)||!quick_atomic_attempted||strcmp(key,quick_atomic_key))return false;
 int rc=restore?(!strcmp(key,PQA_VOLUME_KEY)?portable_preference_atomic_prepare_volume_off(&quick_atomic_edit,value,size,*restore):
  portable_preference_atomic_prepare_brightness_off(&quick_atomic_edit,value,size,*restore)):
  portable_preference_atomic_prepare(&quick_atomic_edit,value,size);
 if(rc!=PORTABLE_PREFERENCE_READY){quick_atomic_result(rc);return false;}
 quick_atomic_running=true;bool cancel=false;
 for(;;) {
  if(!quick_atomic_guard(NULL)){rc=RISC_ATOMIC_RETAINED;break;}
  rc=cancel?portable_preference_atomic_cancel(&quick_atomic_edit):
   quick_atomic_edit.token?portable_preference_atomic_collect(&quick_atomic_edit):
   portable_preference_atomic_submit(&quick_atomic_edit);
  if(rc!=RISC_ATOMIC_PENDING&&rc!=PORTABLE_PREFERENCE_READY)break;
  /* Native owns a copied payload. Continue ordered input solely to notice
   * Back/Home; suppress new Quick edits until this operation is collected. */
  rt->yield_ms(1);
  if(!cancel) {
   input_service();input_dispatch();
   if(home_pending||handoff_requested||(navigation_pending&T5_APP_BUTTON_BACK))cancel=true;
  }
 }
 quick_atomic_running=false;
 if(!quick_atomic_result(rc))return false;
 bool saved=rc==RISC_ATOMIC_OK;
 pqa_cancel_input(&quick.ui);
 /* Only our collected successful operation advances the next draft's base.
  * No external revision is adopted and no failed operation is replayed. */
 if(saved)quick_atomic_revision=quick_atomic_edit.receipt.revision;
 quick_atomic_edit=(portable_preference_atomic){0};quick_atomic_attempted=false;quick_atomic_key[0]=0;
 rt->diagnostic(saved?"QUICK preference=native-confirmed sd=unconfirmed":
  rc==RISC_ATOMIC_STALE?"QUICK preference=stale reopen-control":
  rc==RISC_ATOMIC_UNCERTAIN?"QUICK preference=uncertain reopen-control":"QUICK preference=not-saved");
 return saved;
}
