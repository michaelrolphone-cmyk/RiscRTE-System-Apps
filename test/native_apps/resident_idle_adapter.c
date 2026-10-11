#include "../../lib/PortableApps/src/adapter.c"
void idle_test_fail(void){failed=true;}
void idle_test_retain(void){portable_adapter_retain_silent();}
bool idle_test_flipped(void){return paper_flip_ui;}
