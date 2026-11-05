/* Test-only observation; text bytes are deliberately never logged. */
#undef t5_driver_get
#define t5_driver_get wifi_raw_text_driver_get
#include "../../Services/text_input/host.c"
#undef t5_driver_get
#include <stdio.h>
void wifi_text_trace(void){
    fprintf(stderr,"WIFI_TEXT_TRACE token=%llu scene=%llu state=%u revision=%u document_revision=%u changed=%u closing=%u checkpoint_paused=%u checkpoint_pausing=%u full_pause=%u\n",
        (unsigned long long)session,(unsigned long long)scene_session,state.state,state.revision,document.revision,
        changed,closing,checkpoint_paused,checkpoint_pausing,checkpoint_full_pause);
}
const risc_driver_v2 *text_driver_get(uint32_t abi){return wifi_raw_text_driver_get(abi);}
