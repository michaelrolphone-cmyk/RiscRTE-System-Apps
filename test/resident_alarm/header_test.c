/* Header/layout compatibility only; no simulated host or Runtime behavior. */
#include "RiscResidentShellV1.h"
#ifdef TEST_ALARM_CONTRACT
#include "RiscResidentIdleV1.h"
#include "RiscResidentAlarmV1.h"
#endif
#include <stdio.h>
#ifdef __cplusplus
#define CHECK static_assert
#else
#define CHECK _Static_assert
#endif
CHECK(sizeof(risc_resident_request_v1)==16, "request prefix changed");
CHECK(sizeof(risc_resident_reply_v1)==8, "reply prefix changed");
CHECK(sizeof(risc_resident_app_descriptor_v1_t)==16, "descriptor changed");
CHECK(RISC_RESIDENT_CHECKPOINT_ALARM==3, "existing alarm reason changed");
#ifdef TEST_ALARM_CONTRACT
CHECK(RISC_RESIDENT_QUERY_ALARM_ONLY==UINT32_C(0x414c5201), "alarm query");
CHECK(RISC_RESIDENT_QUERY_LAST_IDLE_ALARM==UINT32_C(0x414c5202), "outcome query");
CHECK(RISC_RESIDENT_QUERY_ALARM_ONLY!=RISC_RESIDENT_QUERY_LAST_IDLE_ALARM, "collision");
CHECK(RISC_RESIDENT_QUERY_ALARM_ONLY>RISC_RESIDENT_CHECKPOINT_POLICY, "legacy collision");
CHECK(RISC_RESIDENT_QUERY_LAST_IDLE_ALARM>RISC_RESIDENT_CHECKPOINT_POLICY, "legacy collision");
CHECK(RISC_RESIDENT_QUERY_ALARM_ONLY!=RISC_RESIDENT_QUERY_IDLE_ONLY, "idle query collision");
CHECK(RISC_RESIDENT_QUERY_ALARM_ONLY!=RISC_RESIDENT_APPLY_IDLE_ONLY, "idle apply collision");
CHECK(RISC_RESIDENT_QUERY_LAST_IDLE_ALARM!=RISC_RESIDENT_QUERY_IDLE_ONLY, "idle query collision");
CHECK(RISC_RESIDENT_QUERY_LAST_IDLE_ALARM!=RISC_RESIDENT_APPLY_IDLE_ONLY, "idle apply collision");
#endif
#define SIZE(T) printf(#T "=%zu\n",sizeof(T))
#define OFFSET(T,F) printf(#T "." #F "=%zu\n",offsetof(T,F))
int main(void){
 SIZE(risc_resident_client_v1);
 OFFSET(risc_resident_client_v1,api_version);
 OFFSET(risc_resident_client_v1,struct_size);
 OFFSET(risc_resident_client_v1,invocation);
 OFFSET(risc_resident_client_v1,role);
 OFFSET(risc_resident_client_v1,reserved);
 OFFSET(risc_resident_client_v1,register_shell);
 OFFSET(risc_resident_client_v1,run_foreground);
 OFFSET(risc_resident_client_v1,checkpoint);
 OFFSET(risc_resident_client_v1,request_foreground_exit);
 OFFSET(risc_resident_client_v1,last_failure);
 SIZE(risc_resident_request_v1);SIZE(risc_resident_reply_v1);
 SIZE(risc_resident_app_descriptor_v1_t);SIZE(risc_resident_callbacks_v1);
 printf("callbacks_prefix=%zu\n",(size_t)RISC_RESIDENT_CALLBACKS_V1_SIZE);
 printf("callbacks_loading=%zu\n",(size_t)RISC_RESIDENT_CALLBACKS_LOADING_V1_SIZE);
 return 0;
}
