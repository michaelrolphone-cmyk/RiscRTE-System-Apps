/* Read-only Quick observations and a test-only orderly end to Home's loop. */
#include "shared_quick_reference.h"
#ifdef REFERENCE_RUNTIME_FIXTURE
#include <stdlib.h>
extern void reference_free(void*);
#define free reference_free
#endif
#include "../../lib/PortableApps/src/adapter.c"
#ifdef PORTABLE_RESIDENT_SHELL_HOST
void reference_observe_ui(reference_ui*out){*out=(reference_ui){quick_modal,pqa_visible(&quick.ui),quick.ui.neutral_gate,(unsigned)quick.ui.position_q8,quick.ui.brightness,quick.ui.clean_refresh_valid,quick.ui.audio_controls};}
bool reference_ui_point(unsigned control,int*x,int*y){
 if(control==100){*x=364;*y=148;return true;}
 if(control==7&&!quick.ui.audio_controls){*x=240;*y=550;return true;}
 if(!pqa_paper_tile(&quick.ui,control,x,y))return false;
 *x+=102;*y+=46;return true;
}
void reference_finish_home(void){handoff_requested=true;}
#endif
