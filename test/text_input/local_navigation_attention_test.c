/* Production attention() body is extracted by the runner, not reimplemented. */
#include "RiscInputNavigationV1.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#define RISC_TEXT_ENTRY_INVALID -2
static bool live=true,text_input_suspended=true,navigation_ready=true,navigation_neutral;
static bool handoff_requested,exit_pending,nav_ok=true,lose_after_poll,alarm_owned;
static unsigned polls,pumps,retentions;
static const void *rt;
static risc_input_navigation_frame_v1 next_frame;
static struct {struct {bool output_uncertain;unsigned state;} status;} alarms;
#define ALARM_STATE_BLOCKED 1u
#define ALARM_STATE_CUE 2u
static bool text_input_alive(void){return live;}
static int text_input_failure(void){retentions++;return -3;}
static bool risc_runtime_exit_requested_v1(const void *p){(void)p;return exit_pending;}
static bool alarm_audio_pump(void){pumps++;return true;}
static bool portable_alarm_owned(const void *p){(void)p;return alarm_owned;}
static bool poll(void*c,risc_input_navigation_frame_v1*f){(void)c;polls++;*f=next_frame;if(lose_after_poll)live=false;return nav_ok;}
static const risc_input_navigation_api_v1 api={.api_version=1,.struct_size=sizeof(api),.poll=poll};
static const risc_input_navigation_api_v1 *navigation=&api;
#include "attention-body.inc"
int main(void){
 assert(!portable_text_adapter_attention());
#ifdef PORTABLE_INPUT_NAVIGATION_LOCAL
 assert(polls==1&&navigation_neutral);
 next_frame=(risc_input_navigation_frame_v1){.pressed=RISC_NAV_BACK};assert(portable_text_adapter_attention()==1);
 navigation_neutral=false;assert(!portable_text_adapter_attention());assert(!navigation_neutral);
 next_frame=(risc_input_navigation_frame_v1){0};assert(!portable_text_adapter_attention()&&navigation_neutral);
 next_frame.pressed=RISC_NAV_BACK;assert(portable_text_adapter_attention()==1);
 navigation_ready=false;unsigned count=polls;assert(!portable_text_adapter_attention()&&polls==count);navigation_ready=true;
 next_frame=(risc_input_navigation_frame_v1){0};nav_ok=false;assert(portable_text_adapter_attention()==-3&&retentions==1);nav_ok=true;
 lose_after_poll=true;assert(portable_text_adapter_attention()==-3&&retentions==2);lose_after_poll=false;live=true;
#else
 next_frame.pressed=RISC_NAV_BACK;assert(!portable_text_adapter_attention()&&!polls);
#endif
#ifdef PORTABLE_ALARM_CLIENT
 unsigned before=polls;alarm_owned=true;assert(portable_text_adapter_attention()==1&&polls==before);alarm_owned=false;
 alarms.status.output_uncertain=true;assert(portable_text_adapter_attention()==-3&&polls==before);alarms.status.output_uncertain=false;
#endif
#ifdef PORTABLE_TEXT_INPUT_POLICY
 unsigned p0=polls,a0=pumps;exit_pending=true;assert(portable_text_adapter_attention()==1&&polls==p0&&pumps==a0);exit_pending=false;
 handoff_requested=true;assert(portable_text_adapter_attention()==1&&polls==p0&&pumps==a0);handoff_requested=false;
#endif
 unsigned p1=polls,a1=pumps;live=false;assert(portable_text_adapter_attention()==-3&&polls==p1&&pumps==a1);live=true;
 text_input_suspended=false;assert(portable_text_adapter_attention()==RISC_TEXT_ENTRY_INVALID&&polls==p1&&pumps==a1);
 puts("PASS production local-navigation attention: neutral Back, failure fences, alarm/exit priority and no shared-navigation consumption");return 0;
}
