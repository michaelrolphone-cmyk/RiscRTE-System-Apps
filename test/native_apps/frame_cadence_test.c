#include "PortableFrameCadence.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 unsigned rgb=RISC_DISPLAY_FORMAT_RGB565;
 assert(portable_frame_cadence_ms(rgb,0,0,40)==20);
 assert(portable_frame_cadence_ms(rgb,0,60000,40)==17);
 assert(portable_frame_cadence_ms(rgb,0,59940,40)==17);
 assert(portable_frame_cadence_ms(rgb,0,50000,40)==20);
 assert(portable_frame_cadence_ms(rgb,0,40000,40)==25);
 assert(portable_frame_cadence_ms(rgb,0,30000,40)==34);
 assert(portable_frame_cadence_ms(rgb,0,1,40)==40);
 assert(portable_frame_cadence_ms(rgb,0,UINT32_MAX,40)==17);
 assert(portable_frame_cadence_ms(rgb,RISC_DISPLAY_INFO_RETAINS_IMAGE,60000,40)==40);
 assert(portable_frame_cadence_ms(RISC_DISPLAY_FORMAT_MONO1,0,60000,40)==40);
 assert(portable_frame_cadence_ms(RISC_DISPLAY_FORMAT_GRAY4,0,60000,40)==40);
 assert(portable_frame_cadence_ms(0,0,60000,40)==40);
 assert(portable_frame_cadence_ms(rgb,0,0,0)==20);
 assert(portable_frame_cadence_ms(rgb,0,0,10)==10);
 assert(portable_frame_cadence_ms(rgb,0,60000,10)==10);
 puts("Frame cadence: capabilities, legacy, ceil, extremes, never slow caller PASS");return 0;
}
