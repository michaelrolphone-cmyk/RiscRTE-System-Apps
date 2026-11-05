/* input.text is an existing transport event ABI. ui.text-input is the separate
 * System-owned modal session ABI. Both headers must coexist unchanged. */
#include "fixtures/RiscTextInputV1.h"
#include "RiscTextEntryV1.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* Frozen v1 prefix shapes: adding a reason must move no old callback, copied
 * request field, or result field. Compile these on host and target. */
typedef struct { uint32_t api_version,struct_size,capacity,reserved; char label[40],text[72]; } old_request;
typedef struct { uint32_t struct_size,state,flags,revision; char text[72]; } old_state;
typedef struct { uint32_t api_version,struct_size; void *context;
 int32_t (*open)(void*,const risc_text_entry_request_v1*,uint64_t*);
 int32_t (*poll)(void*,uint64_t,risc_text_entry_state_v1*);
 int32_t (*close)(void*,uint64_t); } old_api;
#define SAME_SIZE(a,b) _Static_assert(sizeof(a)==sizeof(b),"old layout size")
#define SAME_OFFSET(a,b,f) _Static_assert(offsetof(a,f)==offsetof(b,f),"old layout field")
SAME_SIZE(old_request,risc_text_entry_request_v1);SAME_SIZE(old_state,risc_text_entry_state_v1);SAME_SIZE(old_api,risc_text_entry_api_v1);
SAME_OFFSET(old_request,risc_text_entry_request_v1,reserved);SAME_OFFSET(old_request,risc_text_entry_request_v1,text);
SAME_OFFSET(old_state,risc_text_entry_state_v1,flags);SAME_OFFSET(old_state,risc_text_entry_state_v1,text);
SAME_OFFSET(old_api,risc_text_entry_api_v1,context);SAME_OFFSET(old_api,risc_text_entry_api_v1,open);
SAME_OFFSET(old_api,risc_text_entry_api_v1,poll);SAME_OFFSET(old_api,risc_text_entry_api_v1,close);
_Static_assert(offsetof(risc_text_entry_api_v1_home_reason,base)==0,"base remains prefix");
_Static_assert(offsetof(risc_text_entry_api_v1_home_reason,home_reason_tag)==sizeof(old_api),"suffix follows prefix");
/* Frozen 128-byte extension, including all existing suffix offsets. */
typedef struct { uint32_t api_version,struct_size,capacity,reserved; char label[40],text[128]; } old_extended_request;
typedef struct { uint32_t struct_size,state,flags,revision; char text[128]; } old_extended_state;
typedef struct { old_api base; uint32_t home_reason_tag,home_reason_version; } old_home;
typedef struct { old_home base; uint32_t masked_tag,masked_version; } old_masked;
typedef struct { old_masked base; uint32_t extended_tag,extended_version;
 int32_t (*open_extended)(void*,const risc_text_entry_request_v1_extended*,uint64_t*);
 int32_t (*poll_extended)(void*,uint64_t,risc_text_entry_state_v1_extended*); } old_extended_api;
SAME_SIZE(old_extended_request,risc_text_entry_request_v1_extended);
SAME_SIZE(old_extended_state,risc_text_entry_state_v1_extended);
SAME_SIZE(old_home,risc_text_entry_api_v1_home_reason);
SAME_SIZE(old_masked,risc_text_entry_api_v1_masked);
SAME_SIZE(old_extended_api,risc_text_entry_api_v1_extended);
SAME_OFFSET(old_extended_request,risc_text_entry_request_v1_extended,text);
SAME_OFFSET(old_extended_state,risc_text_entry_state_v1_extended,text);
SAME_OFFSET(old_masked,risc_text_entry_api_v1_masked,masked_tag);
SAME_OFFSET(old_extended_api,risc_text_entry_api_v1_extended,extended_tag);
SAME_OFFSET(old_extended_api,risc_text_entry_api_v1_extended,open_extended);
SAME_OFFSET(old_extended_api,risc_text_entry_api_v1_extended,poll_extended);
_Static_assert(offsetof(risc_text_entry_api_v1_long,base)==0,"extended remains prefix");
_Static_assert(offsetof(risc_text_entry_api_v1_long,long_tag)==sizeof(old_extended_api),"long suffix follows frozen extension");
/* The checkpoint suffix preserves the full frozen long-text provider shape. */
typedef struct {old_extended_api base;uint32_t long_tag,long_version;
 int32_t (*open_long)(void*,const risc_text_entry_request_v1_long*,uint64_t*);
 int32_t (*poll_long)(void*,uint64_t,risc_text_entry_state_v1_long*);} old_long_api;
SAME_SIZE(old_long_api,risc_text_entry_api_v1_long);
SAME_OFFSET(old_long_api,risc_text_entry_api_v1_long,long_tag);
SAME_OFFSET(old_long_api,risc_text_entry_api_v1_long,open_long);
SAME_OFFSET(old_long_api,risc_text_entry_api_v1_long,poll_long);
_Static_assert(offsetof(risc_text_entry_api_v1_checkpoint,base)==0,"long remains prefix");
_Static_assert(offsetof(risc_text_entry_api_v1_checkpoint,checkpoint_tag)==sizeof(old_long_api),"checkpoint follows frozen long suffix");
int main(void){
 risc_text_input_api_v1 transport={.api_version=RISC_TEXT_INPUT_API_V1};
 risc_text_entry_api_v1 session={.api_version=RISC_TEXT_ENTRY_API_V1};
 assert(transport.api_version==1&&session.api_version==1);
 assert(!strcmp(RISC_TEXT_ENTRY_CAPABILITY,"ui.text-input"));
 assert(RISC_TEXT_EVENT_CONNECTED==1&&RISC_TEXT_ENTRY_PRESENTING==2);
 puts("transport text and modal text-entry ABIs coexist PASS");return 0;
}
