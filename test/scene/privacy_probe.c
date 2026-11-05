/* Compile the production scene owner unchanged, with a test-only observer. */
#include "../../Services/scene_host/host.c"
int scene_private_storage_cleared(void){
    risc_components_document_v1 empty={0};
    return !memcmp(&logical_model.doc,&empty,sizeof(empty))&&
        !memcmp(&frame_model.doc,&empty,sizeof(empty));
}
