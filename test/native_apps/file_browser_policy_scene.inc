#include "RiscSceneCheckpointV1.h"
#include "PortableQuickRadios.h"
static bool policy_scene_paused,policy_scene_again,policy_resume_retained,policy_exit,policy_scene_missing;
static unsigned policy_scene_features,policy_pauses,policy_resumes,policy_battery_reads;
static uint8_t policy_percent=70;
static bool policy_battery_read(void*c,risc_battery_sample_v1*out){(void)c;policy_battery_reads++;*out=(risc_battery_sample_v1){3900,policy_percent,0};return true;}
static const risc_battery_gauge_api_v1 policy_battery={1,sizeof(policy_battery),NULL,policy_battery_read};
static int32_t policy_configure(void*c,uint64_t s,uint32_t flags){(void)c;assert(s==11);policy_scene_features=flags;return policy_scene_missing?RISC_SCENE_INVALID:RISC_SCENE_OK;}
static int32_t policy_component_open(void*c,const risc_components_document_v1*d,const risc_scene_navigation_v1*n,uint64_t*s){(void)c;(void)d;(void)n;(void)s;assert(false);return -1;}
static int32_t policy_component_update(void*c,uint64_t s,const risc_components_document_v1*d){(void)c;(void)s;(void)d;assert(false);return -1;}
static int32_t policy_geometry(void*c,risc_scene_page_geometry_v1*g){(void)c;(void)g;assert(false);return -1;}
static int32_t policy_page(void*c,uint64_t s,const risc_scene_page_document_v1*d,const uint8_t*p,size_t z){(void)c;(void)s;(void)d;(void)p;(void)z;assert(false);return -1;}
static int32_t policy_refresh(void*c,uint64_t s,const risc_scene_page_document_v1*d,const uint8_t*p,size_t z,uint32_t f){(void)f;return policy_page(c,s,d,p,z);}
static int32_t policy_pause(void*c,uint64_t s){(void)c;assert(s==11&&!policy_scene_paused);policy_pauses++;if(policy_scene_again)return RISC_SCENE_AGAIN;policy_scene_paused=true;return 0;}
static int32_t policy_resume(void*c,uint64_t s,uint32_t flags){(void)c;(void)flags;assert(s==11&&policy_scene_paused);policy_resumes++;if(policy_resume_retained)return RISC_SCENE_RETAINED;policy_scene_paused=false;return 0;}
static risc_scene_checkpoint_api_v1 text_scene={.presentation={.page={.components={.lifecycle={.base={1,sizeof(text_scene),NULL,text_scene_open,text_scene_update,text_scene_next,NULL,text_scene_snapshot,text_scene_close},.configure=policy_configure},.tag=RISC_COMPONENTS_TAG,.version=1,.open=policy_component_open,.update=policy_component_update},.page_tag=RISC_SCENE_PAGE_TAG,.page_version=1,.geometry=policy_geometry,.present_page=policy_page},.refresh_tag=RISC_SCENE_PAGE_REFRESH_TAG,.refresh_version=1,.present_page_with_refresh=policy_refresh},.checkpoint_tag=RISC_SCENE_CHECKPOINT_TAG,.checkpoint_version=1,.pause=policy_pause,.resume=policy_resume};
static bool policy_exit_requested(void){return policy_exit;}

static bool policy_ble_on;
static bool policy_ble_enable(void*c,bool on){(void)c;policy_ble_on=on;return true;}
static bool policy_ble_status(void*c,uint8_t*s){(void)c;*s=policy_ble_on?PORTABLE_BLUETOOTH_ON:PORTABLE_BLUETOOTH_OFF;return true;}
static const portable_bluetooth_control_v1 policy_ble={1,sizeof(policy_ble),NULL,NULL,NULL,policy_ble_enable,policy_ble_status};

#ifdef FILE_POLICY_RESIDENT
static unsigned policy_resident_polls,policy_resident_dispatches;
static bool policy_resident_exit;
static int32_t policy_resident_checkpoint(uint64_t token,const risc_resident_request_v1*q,risc_resident_reply_v1*r){
 assert(token==42&&!owned&&!fb_owned[0].handle);
 *r=(risc_resident_reply_v1){sizeof(*r),0};
 if(q->reason==RISC_RESIDENT_CHECKPOINT_POLL){policy_resident_polls++;if(!(q->flags&RISC_RESIDENT_POLL_INHIBIT_POLICY))r->flags=RISC_RESIDENT_REPLY_POLICY_REQUEST|PORTABLE_RESIDENT_REPLY_INPUT_CONTINUITY;return RISC_RESIDENT_OK;}
 assert(q->reason==RISC_RESIDENT_CHECKPOINT_POLICY&&!fb_grant.api);
 if(fb_text_active)assert(fb_text_checkpoint.paused&&fb_text_policy_boundary);
 policy_resident_dispatches++;return policy_resident_exit?RISC_RESIDENT_EXIT:RISC_RESIDENT_OK;
}
static bool policy_resident_bind(risc_resident_client_v1*out){*out=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(*out),.invocation=42,.role=RISC_RESIDENT_ROLE_FOREGROUND,.checkpoint=policy_resident_checkpoint};return true;}
#endif

static unsigned policy_background;
#ifdef PORTABLE_CONTEXTS_CLIENT
static unsigned policy_contexts;
#endif
static bool fake_broadcast_pause(void*c){live_check();(void)c;return true;}
static bool fake_broadcast_step(void*c,bool allow,const telemetry_broadcast_policy_v1*p){live_check();if(fb_text_active)policy_background++;(void)c;(void)allow;(void)p;return true;}
static bool fake_broadcast_status(void*c,telemetry_broadcast_status_v1*s){live_check();(void)c;*s=(telemetry_broadcast_status_v1){.struct_size=sizeof(*s),.state=TELEMETRY_BROADCAST_OFF};return true;}
static int32_t fake_broadcast_enum(void*c,uint32_t i,risc_telemetry_field_v1*f){live_check();(void)c;(void)i;(void)f;return 0;}
static int32_t fake_broadcast_read(void*c,uint32_t i,int32_t*v){live_check();(void)c;(void)i;(void)v;return 0;}
static const telemetry_broadcast_v1 broadcast_api={1,sizeof(broadcast_api),NULL,fake_broadcast_step,fake_broadcast_pause,fake_broadcast_status,fake_broadcast_enum,fake_broadcast_read};
#ifdef PORTABLE_CONTEXTS_CLIENT
/* Bounded base Contexts service: no background sensors, presets or exports.
 * The actual app-data-backed shared fingerprint pipeline remains in the app. */
static bool ctx_step(void*c,const contexts_policy_v1*p){live_check();if(fb_text_active)policy_contexts++;(void)c;(void)p;return true;}
static bool ctx_pause(void*c){live_check();(void)c;return true;}
static bool ctx_status(void*c,contexts_status_v1*s){live_check();(void)c;*s=(contexts_status_v1){.struct_size=sizeof(*s),.state=CONTEXTS_PAUSED};return true;}
static bool ctx_source(void*c,uint32_t s){live_check();(void)c;(void)s;return true;}
static bool ctx_record(void*c,uint32_t s,uint32_t k,uint32_t i,const void*b,uint32_t n){live_check();(void)c;(void)s;(void)k;(void)i;(void)b;(void)n;return true;}
static bool ctx_finish(void*c,uint32_t s,uint32_t r){live_check();(void)c;(void)s;(void)r;return true;}
static int32_t ctx_label(void*c,uint32_t s,uint32_t i,contexts_label_v1*l){live_check();(void)c;(void)s;(void)i;(void)l;return 0;}
static bool ctx_claim(void*c,uint32_t s,uint32_t i,const char*n,uint32_t g){live_check();(void)c;(void)s;(void)i;(void)n;(void)g;return false;}
static bool ctx_result(void*c,uint32_t s,uint32_t g,uint32_t r){live_check();(void)c;(void)s;(void)g;(void)r;return true;}
static const contexts_service_v1 contexts_api={.api_version=1,.struct_size=sizeof(contexts_api),.step=ctx_step,.pause=ctx_pause,.status=ctx_status,.request_export=ctx_source,.begin_export=ctx_source,.export_record=ctx_record,.finish_export=ctx_finish,.label=ctx_label,.claim_preset=ctx_claim,.preset_result=ctx_result,.capture_audio=ctx_pause};
#endif
