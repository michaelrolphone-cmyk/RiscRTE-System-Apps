/* Compile the production text owner unchanged, with a test-only observer. */
#include "../../Services/text_input/host.c"
int text_private_storage_cleared(void){
    risc_text_entry_state_v1 empty_state={0};risc_scene_document_v1 empty_document={0};
    return !revealed&&!memcmp(&state,&empty_state,sizeof(state))&&
        !memcmp(&document,&empty_document,sizeof(document));
}
