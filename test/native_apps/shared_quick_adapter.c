/* Read-only Quick observations and a test-only orderly end to Home's loop. */
#include "shared_quick_reference.h"
#ifdef REFERENCE_RUNTIME_FIXTURE
#include <stdlib.h>
extern void reference_free(void*);
#define free reference_free
#endif
#include "../../lib/PortableApps/src/adapter.c"
#ifdef PORTABLE_RESIDENT_SHELL_HOST
void reference_observe_ui(reference_ui*out){*out=(reference_ui){.modal=quick_modal,.visible=pqa_visible(&quick.ui),.neutral=quick.ui.neutral_gate,.position=(unsigned)quick.ui.position_q8,.brightness=quick.ui.brightness,.clean=quick.ui.clean_refresh_valid,.audio=quick.ui.audio_controls};
#ifdef PORTABLE_FRONTLIGHT_TONE
 out->tone_controls=quick.ui.tone_controls;out->tone_valid=quick.ui.tone_valid&&quick.ui.applied_tone_valid;out->tone=quick.ui.applied_tone;
#endif
}
bool reference_ui_point(unsigned control,int*x,int*y){
 if(control==100){*x=364;*y=pqa_paper_slider_y(&quick.ui,0)+15;return true;}
 if(control>=101&&control<=103){int top=pqa_paper_slider_y(&quick.ui,2);if(!top)return false;
  *x=control==101?88:control==102?364:226;*y=top+15;return true;}
 if(!pqa_paper_tile(&quick.ui,control,x,y))return false;
 *x+=102;*y+=46;return true;
}
void reference_finish_home(void){handoff_requested=true;}
#endif
