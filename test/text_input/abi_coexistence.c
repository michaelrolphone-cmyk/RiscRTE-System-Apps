/* input.text is an existing transport event ABI. ui.text-input is the separate
 * System-owned modal session ABI. Both headers must coexist unchanged. */
#include "fixtures/RiscTextInputV1.h"
#include "RiscTextEntryV1.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 risc_text_input_api_v1 transport={.api_version=RISC_TEXT_INPUT_API_V1};
 risc_text_entry_api_v1 session={.api_version=RISC_TEXT_ENTRY_API_V1};
 assert(transport.api_version==1&&session.api_version==1);
 assert(!strcmp(RISC_TEXT_ENTRY_CAPABILITY,"ui.text-input"));
 assert(RISC_TEXT_EVENT_CONNECTED==1&&RISC_TEXT_ENTRY_PRESENTING==2);
 puts("transport text and modal text-entry ABIs coexist PASS");return 0;
}
