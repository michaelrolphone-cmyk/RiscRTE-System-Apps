#include "shared_quick_adapter.c"
uint32_t home_idle_last_activity(void){return last_activity;}
uint32_t home_idle_delay(void){return portable_idle_ms();}
