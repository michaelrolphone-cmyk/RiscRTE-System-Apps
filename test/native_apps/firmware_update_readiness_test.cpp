/* Policy discovery is optional, read-only and independent of transport. */
#include <cassert>
#include <initializer_list>
#include <cstdlib>
#include <cstring>
#include "../../Services/update/service.cpp"
struct old_update_v1 {
 uint32_t api_version,struct_size;
 void *context;
 bool (*refresh)(void*,uint64_t);
 bool (*step)(void*);
 bool (*status)(void*,software_update_status_v1*);
 bool (*get)(void*,uint32_t,software_update_row_v1*);
 bool (*begin)(void*,uint32_t,uint64_t);
 bool (*cancel)(void*);
 bool (*activate)(void*);
 bool (*restart)(void*);
};
static_assert(sizeof(old_update_v1)==SOFTWARE_UPDATE_V1_PREFIX_SIZE,"legacy prefix size");
#define SAME_OFFSET(member) static_assert(offsetof(old_update_v1,member)==offsetof(software_update_v1,member),"legacy " #member)
SAME_OFFSET(api_version); SAME_OFFSET(struct_size); SAME_OFFSET(context);
SAME_OFFSET(refresh); SAME_OFFSET(step); SAME_OFFSET(status); SAME_OFFSET(get);
SAME_OFFSET(begin); SAME_OFFSET(cancel); SAME_OFFSET(activate); SAME_OFFSET(restart);
static unsigned calls;
static int32_t h_open(void*,const risc_http_request_v1*,uint64_t*){++calls;return RISC_HTTP_INVALID;}
static int32_t h_read(void*,uint64_t,void*,uint32_t,uint32_t*){++calls;return RISC_HTTP_INVALID;}
static int32_t h_info(void*,uint64_t,risc_http_response_v1*){++calls;return RISC_HTTP_INVALID;}
static int32_t h_close(void*,uint64_t){++calls;return RISC_HTTP_INVALID;}
static bool b_status(void*,risc_bank_status_v1*){++calls;return false;}
static int32_t b_image(void*,const risc_bank_image_v1*,uint64_t*){++calls;return RISC_BANK_UNAVAILABLE;}
static int32_t b_app(void*,const char*,const void*,uint32_t,const risc_bank_image_v1*,uint64_t*){++calls;return RISC_BANK_UNAVAILABLE;}
static int32_t b_step(void*,uint64_t,risc_bank_status_v1*){++calls;return RISC_BANK_UNAVAILABLE;}
static int32_t b_write(void*,uint64_t,const void*,uint32_t){++calls;return RISC_BANK_UNAVAILABLE;}
static int32_t b_handle(void*,uint64_t){++calls;return RISC_BANK_UNAVAILABLE;}
static bool b_restart(void*,uint64_t){++calls;return false;}
static int32_t b_get(void*,uint32_t,void*,uint32_t,uint32_t*){++calls;return RISC_BANK_UNAVAILABLE;}
static uint64_t time_ms(void*){++calls;return 0;}
static void sleep_ms(void*,uint32_t){++calls;}
int main(){
 const software_update_v1 *a=(const software_update_v1*)t5_driver_get(2)->capability;
 const auto *m=software_update_metadata_get_v1(a);assert(m);
 assert(!software_update_metadata_get_v1(nullptr));
 for(size_t n=2*sizeof(uint32_t);n<sizeof(software_update_metadata_api_v1);++n){
  auto *short_api=(software_update_v1*)malloc(n);assert(short_api);
  memcpy(short_api,a,n);short_api->struct_size=(uint32_t)n;
  assert(!software_update_metadata_get_v1(short_api));free(short_api);
 }
 software_update_metadata_api_v1 bad=*m;
 bad.base.api_version=2;assert(!software_update_metadata_get_v1(&bad.base));bad=*m;
 bad.tag^=1;assert(!software_update_metadata_get_v1(&bad.base));bad=*m;
 bad.version=2;assert(!software_update_metadata_get_v1(&bad.base));bad=*m;
 bad.readiness=nullptr;assert(!software_update_metadata_get_v1(&bad.base));
 software_update_readiness_v1 r={};r.struct_size=sizeof(r);
 assert(!m->readiness(m->base.context,&r));
 const risc_http_client_v1 h={1,sizeof(h),nullptr,h_open,h_read,h_info,h_close};
 const risc_bank_store_v1 b={1,sizeof(b),nullptr,b_status,b_image,b_app,b_step,b_write,b_handle,b_handle,b_handle,b_restart,b_get,nullptr,nullptr};
 const risc_platform_clock_api_v1 t={1,sizeof(t),nullptr,time_ms,sleep_ms};
 const risc_provider_dependency_v1 deps[]={{RISC_HTTP_CLIENT_CAPABILITY,1,&h},{RISC_BANK_STORE_CAPABILITY,1,&b},{"platform.clock",1,&t}};
 assert(start(deps,3));assert(!m->readiness(m->base.context,nullptr));
 for(uint32_t n=0;n<sizeof(r);++n){r.struct_size=n;assert(!m->readiness(m->base.context,&r));assert(r.struct_size==n);}
 r.struct_size=sizeof(r);
 const auto before=view;
 for(int i=0;i<3;++i){assert(m->readiness(m->base.context,&r));assert(r.configured==(WatchUpdate::CatalogUrl[0]!=0));assert(r.network_required==r.configured);assert(!strcmp(r.reason,r.configured?"Update feed requires network access":"No update feed configured"));}
 assert(!memcmp(&before,&view,sizeof(view))&&!http_handle&&!bank_handle&&calls==0);
 /* Discovery cannot close or alter retained and activation-unknown resources. */
 for(uint32_t state:{SOFTWARE_UPDATE_RETAINED,SOFTWARE_UPDATE_ACTIVATION_UNKNOWN}){
  view.state=state;http_handle=17;bank_handle=23;
  assert(m->readiness(m->base.context,&r));assert(view.state==state&&http_handle==17&&bank_handle==23&&calls==0);
 }
 http_handle=bank_handle=0;view=before;
 if(!r.configured){assert(refresh(nullptr,0));assert(view.state==SOFTWARE_UPDATE_LIST&&calls==0);}
 assert(quiesce());assert(!m->readiness(m->base.context,&r));assert(calls==0);
 puts("firmware update readiness passed");
}
