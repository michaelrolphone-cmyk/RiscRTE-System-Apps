#pragma once
#include "ContextsRules.h"
/* Separate namespace from the legacy Audio/RF banks. Complete-file replace
 * makes Library + rule edits atomic, including rename/delete references. */
#define PORTABLE_CONTEXT_RULES_INSTANCE 4u
static uint8_t portable_rules_bytes[CR_FILE_BYTES],portable_rules_check[CR_FILE_BYTES];
static contexts_rules_v1 portable_rules_request;
static inline bool portable_context_rules_load(portable_contexts_client*c){
 const contexts_fingerprint_service_v1*fp=contexts_fingerprint_api(c->api);if(!fp)return true;
 portable_rules_request=(contexts_rules_v1){.struct_size=sizeof(portable_rules_request),.set=true};cr_init(&portable_rules_request.store);
 c->rules_available=false;c->rules_revision=0;
 c->fingerprint_store=(risc_runtime_capability_v1){.struct_size=sizeof(c->fingerprint_store)};
 if(!c->runtime->acquire(RISC_SHARED_DATA_CAPABILITY,1,PORTABLE_CONTEXT_RULES_INSTANCE,&c->fingerprint_store))return true;
 const risc_app_data_v1*f=c->fingerprint_store.api;if(!f||f->api_version!=1||f->struct_size<sizeof(*f)||!f->stat||!f->read||!f->replace){if(!c->runtime->release(&c->fingerprint_store))return false;memset(&c->fingerprint_store,0,sizeof(c->fingerprint_store));return false;}uint32_t n=0;uint64_t rev=0,actual=0;
 int32_t rc=f->stat(f->context,"context-rules.ctx",&n,&rev);
 if(!rc&&n==CR_FILE_BYTES){uint32_t got=0;rc=f->read(f->context,"context-rules.ctx",rev,portable_rules_bytes,sizeof(portable_rules_bytes),&got,&actual);if(!rc&&(actual!=rev||got!=n||!cr_decode(&portable_rules_request.store,portable_rules_bytes,got)))rc=RISC_APP_DATA_INVALID;}
 else if(rc==RISC_APP_DATA_NOT_FOUND){rc=0;rev=0;}else if(!rc)rc=RISC_APP_DATA_INVALID;
 if(rc==RISC_APP_DATA_RETAINED)return false;
 if(!c->runtime->release(&c->fingerprint_store))return false;
 memset(&c->fingerprint_store,0,sizeof(c->fingerprint_store));c->rules_available=!rc;c->rules_revision=rev;
 portable_rules_request.available=!rc;
 if(!rc&&!fp->fingerprint(fp->base.context,CONTEXTS_FP_RULES,&portable_rules_request))return false;
 return true;
}
static inline bool portable_context_rules_save(portable_contexts_client*c,const cr_store*s){
 if(!c->rules_available||!cr_encode(s,portable_rules_bytes,sizeof(portable_rules_bytes)))return false;
 const contexts_fingerprint_service_v1*fp=contexts_fingerprint_api(c->api);if(!fp||!c->api->pause(c->api->context))return false;
 c->loaded=false;c->fingerprint_store=(risc_runtime_capability_v1){.struct_size=sizeof(c->fingerprint_store)};
 if(!c->runtime->acquire(RISC_SHARED_DATA_CAPABILITY,1,PORTABLE_CONTEXT_RULES_INSTANCE,&c->fingerprint_store))return false;
 const risc_app_data_v1*f=c->fingerprint_store.api;if(!f||f->api_version!=1||f->struct_size<sizeof(*f)||!f->stat||!f->read||!f->replace){if(!c->runtime->release(&c->fingerprint_store))return false;memset(&c->fingerprint_store,0,sizeof(c->fingerprint_store));return false;}
 /* Any app-data write invalidates tokens globally. Refresh the token while
  * checking the saved generation, rather than retrying a stale token forever. */
 uint32_t prior_size=0;uint64_t prior_rev=0;int32_t rc=f->stat(f->context,"context-rules.ctx",&prior_size,&prior_rev);
 if(rc==RISC_APP_DATA_RETAINED)return false;
 bool identical=false;
 if(!rc&&prior_size==CR_FILE_BYTES){uint32_t got=0;uint64_t actual=0;rc=f->read(f->context,"context-rules.ctx",prior_rev,portable_rules_check,sizeof(portable_rules_check),&got,&actual);if(rc==RISC_APP_DATA_RETAINED)return false;
  if(!rc&&(got!=CR_FILE_BYTES||actual!=prior_rev||!cr_decode(&portable_rules_request.store,portable_rules_check,got)))rc=RISC_APP_DATA_INVALID;
  if(!rc){identical=!memcmp(portable_rules_check,portable_rules_bytes,CR_FILE_BYTES);if(!identical&&portable_rules_request.store.generation+1!=s->generation)rc=RISC_APP_DATA_STALE;}
 }else if(rc==RISC_APP_DATA_NOT_FOUND){rc=s->generation==1?0:RISC_APP_DATA_STALE;prior_rev=0;}else if(!rc)rc=RISC_APP_DATA_INVALID;
 if(!rc&&!identical)rc=f->replace(f->context,"context-rules.ctx",prior_rev,portable_rules_bytes,CR_FILE_BYTES);
 if(rc==RISC_APP_DATA_RETAINED)return false;
 uint64_t rev=0;uint32_t n=0;
 /* Resolve a possibly committed write by exact readback. A retry after a
  * lost acknowledgement adopts only the identical intended file. */
 int32_t inspected=f->stat(f->context,"context-rules.ctx",&n,&rev);
 if(inspected==RISC_APP_DATA_RETAINED)return false;
 if(!inspected&&n==CR_FILE_BYTES){uint32_t got=0;uint64_t actual=0;inspected=f->read(f->context,"context-rules.ctx",rev,portable_rules_check,sizeof(portable_rules_check),&got,&actual);if(inspected==RISC_APP_DATA_RETAINED)return false;if(!inspected&&got==CR_FILE_BYTES&&actual==rev&&!memcmp(portable_rules_check,portable_rules_bytes,CR_FILE_BYTES))rc=0;else if(!rc)rc=RISC_APP_DATA_IO;}
 else if(!rc)rc=inspected?inspected:RISC_APP_DATA_IO;
 if(!c->runtime->release(&c->fingerprint_store))return false;
 memset(&c->fingerprint_store,0,sizeof(c->fingerprint_store));if(rc||n!=CR_FILE_BYTES)return false;
 c->rules_revision=rev;portable_rules_request=(contexts_rules_v1){.struct_size=sizeof(portable_rules_request),.set=true,.available=true,.store=*s};
 return fp->fingerprint(fp->base.context,CONTEXTS_FP_RULES,&portable_rules_request);
}
static inline bool portable_context_rules_read(portable_contexts_client*c,cr_store*out){
 const contexts_fingerprint_service_v1*fp=contexts_fingerprint_api(c->api);if(!fp||!out)return false;
 portable_rules_request=(contexts_rules_v1){.struct_size=sizeof(portable_rules_request)};
 if(!fp->fingerprint(fp->base.context,CONTEXTS_FP_RULES,&portable_rules_request)||!portable_rules_request.available)return false;
 *out=portable_rules_request.store;return true;
}
