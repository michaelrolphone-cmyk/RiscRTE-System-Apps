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
int main(void){
 risc_text_input_api_v1 transport={.api_version=RISC_TEXT_INPUT_API_V1};
 risc_text_entry_api_v1 session={.api_version=RISC_TEXT_ENTRY_API_V1};
 assert(transport.api_version==1&&session.api_version==1);
 assert(!strcmp(RISC_TEXT_ENTRY_CAPABILITY,"ui.text-input"));
 assert(RISC_TEXT_EVENT_CONNECTED==1&&RISC_TEXT_ENTRY_PRESENTING==2);
 puts("transport text and modal text-entry ABIs coexist PASS");return 0;
}
