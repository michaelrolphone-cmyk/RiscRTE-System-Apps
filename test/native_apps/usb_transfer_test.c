/* Production app + production adapter. Only capability providers are fixtures. */
#define main baseline_main
#define risc_runtime_get_api baseline_runtime
#define TEST_ASYNC_PRESENT
#include "paper_clock_test.c"
#undef main
#undef risc_runtime_get_api
#include "RiscInputNavigationV1.h"
#include "PortableUsbTransfer.h"
#include <RiscUsbDeviceMscV1.h>
#include <setjmp.h>
#include "RiscDiagnosticCheckpointV1.h"
/* Wall-clock gestures remain stable when rendering captures extra reports. */
#define steps (ms/100u)
static unsigned nav_step=UINT32_MAX;
static unsigned test_case,begins,usb_steps,ends,cancel_ends,confirmed_ends,usb_releases,last_service,queued_until;
static unsigned seen_states,prepare_count,last_prepare_input,prepare_log_lines;
static bool preparing,prepare_failed_clean,prepare_failed_retained;
static unsigned lease_prepare_count,preparing_frames,preparing_wait_polls;
static bool session,configured,release_refused,begin_failed,retained_end;
static const void *selected_api;
static unsigned command_begins,command_ends,diagnostic_reads,media_confirmations;
static jmp_buf retained_exit;
static unsigned checkpoints,checkpoint_requested;static bool checkpoint_revoked;
static char saved_checkpoint[RISC_DIAGNOSTIC_CHECKPOINT_TEXT_MAX+1];
static int32_t checkpoint_write(uint64_t invocation,const char* text,uint32_t length){
 assert(invocation==987 && text && length);if(checkpoint_revoked)return -1;
 ++checkpoints;checkpoint_requested=length;
 const unsigned count=length>RISC_DIAGNOSTIC_CHECKPOINT_TEXT_MAX?RISC_DIAGNOSTIC_CHECKPOINT_TEXT_MAX:length;
 memcpy(saved_checkpoint,text,count);saved_checkpoint[count]=0;
 return length>RISC_DIAGNOSTIC_CHECKPOINT_TEXT_MAX?1:0;
}
static bool checkpoint_get(risc_diagnostic_checkpoint_client_v1* out){
 if(test_case<25 || test_case==28)return false;
 assert(out && out->struct_size==sizeof(*out));*out=(risc_diagnostic_checkpoint_client_v1){1,sizeof(*out),987,checkpoint_write};return true;
}
static const char *capture;
static int32_t usb_begin(void*c,uint64_t*out){
 (void)c;assert(!session&&!frames);begins++;last_service=ms;
 if(test_case==6 && !begin_failed){begin_failed=true;*out=0;return RISC_USB_MSC_REFUSED;}
 session=preparing=true;lease_prepare_count=0;prepare_failed_clean=prepare_failed_retained=false;*out=(test_case==8)?0:42;
 return test_case==4||test_case==8?RISC_USB_MSC_RETAINED:RISC_USB_MSC_OK;
}
static int32_t usb_poll(void*c,uint64_t key,risc_usb_device_msc_status_v1*out){
 (void)c;assert(session&&key==42&&out->struct_size==sizeof(*out));assert(ms-last_service<=2);last_service=ms;usb_steps++;
 if(preparing && test_case!=4){
  if(ms<queued_until)preparing_wait_polls++;
  out->state=RISC_USB_MSC_PREPARING;return RISC_USB_MSC_OK;
 }
 unsigned s=RISC_USB_MSC_WAITING;
 if(test_case!=0 && test_case!=6 && test_case!=11 && test_case!=12){configured=true;s=RISC_USB_MSC_CONNECTED;}
 if(test_case==3 && steps>=7&&steps<35)s=RISC_USB_MSC_SUSPENDED;
 if((test_case==1||test_case==5||test_case==7||test_case==9||test_case>=18)&&steps>=20)s=(test_case==1||test_case==9||test_case>=18)?RISC_USB_MSC_EJECTED:test_case==5?RISC_USB_MSC_MEDIA_UNAVAILABLE:RISC_USB_MSC_DISCONNECTED;
 if(prepare_failed_clean)s=RISC_USB_MSC_MEDIA_UNAVAILABLE;
 if(test_case==4||prepare_failed_retained)s=RISC_USB_MSC_FAULT_RETAINED;
 if(test_case>=18 && steps>=12)out->blocks_read=1;
 out->state=s;out->local_media_ready=s==RISC_USB_MSC_EJECTED||s==RISC_USB_MSC_DISCONNECTED;
 seen_states|=1u<<s;return s==RISC_USB_MSC_FAULT_RETAINED?RISC_USB_MSC_RETAINED:RISC_USB_MSC_OK;
}
static int32_t usb_end(void*c,uint64_t key,uint32_t reason){
 (void)c;assert(session&&key==42&&!frames);ends++;
 if(reason==0)cancel_ends++;else {assert(reason==1);confirmed_ends++;assert(steps>=50);}
 if(configured&&steps<20&&reason==0)return RISC_USB_MSC_REFUSED;
 if((test_case==2||test_case==3||test_case==4)&&reason==0)return RISC_USB_MSC_REFUSED;
 if((test_case==4||test_case==12)&&!retained_end){retained_end=true;return RISC_USB_MSC_RETAINED;}
 session=false;return RISC_USB_MSC_OK;
}
static bool usb_error(void*c,char*out,size_t cap){(void)c;snprintf(out,cap,"Fixture cleanup / media status");return true;}
static int32_t usb_prepare(void*c,uint64_t key){
 (void)c;assert(session&&key==42&&preparing&&!frames&&ms>=queued_until);
 assert(polls!=last_prepare_input);last_prepare_input=polls;prepare_count++;lease_prepare_count++;
 assert(preparing_frames && preparing_wait_polls);
 if(test_case==10)return RISC_USB_MSC_OK;
 if(test_case==11 && begins==1 && lease_prepare_count==2){preparing=false;prepare_failed_clean=true;return RISC_USB_MSC_OK;}
 if(test_case==12 && lease_prepare_count==2){preparing=false;prepare_failed_retained=true;return RISC_USB_MSC_RETAINED;}
 if(test_case>=9 && lease_prepare_count<3)return RISC_USB_MSC_OK;
 preparing=false;return RISC_USB_MSC_OK;
}
static const risc_usb_device_msc_api_v1_prepare msc={
 .base={1,sizeof(msc),NULL,usb_begin,usb_poll,usb_end,usb_error},
 .prepare_tag=RISC_USB_MSC_PREPARE_TAG,.prepare_version=1,.prepare_step=usb_prepare};
static risc_usb_device_msc_api_v1_prepare selected_msc;
#ifdef RISC_USB_MSC_DIAGNOSTICS_TAG
static int32_t usb_protocol(void*c,uint64_t key,risc_usb_device_msc_diagnostics_v1*out){
 (void)c;assert(session&&key==42&&out->struct_size>=sizeof(*out));const uint32_t capacity=out->struct_size;diagnostic_reads++;
 *out=(risc_usb_device_msc_diagnostics_v1){.struct_size=sizeof(*out)};
 if(configured){
  out->commands_started=test_case==23?3:1;out->current_opcode=0x28;out->current_tag=5;
  out->command_bytes=512;out->current_lba=7;out->current_block_count=1;out->command_started_ms=100;
  out->last_poll_gap_ms=2;
  if(steps>=12){out->commands_completed=test_case==23?2:1;out->completed_opcode=0x28;
   out->completed_tag=5;out->last_csw_status=0;out->last_command_elapsed_ms=31;out->last_io_elapsed_ms=12;
   out->last_io_result=0;out->blocks_read=1;out->last_io_lba=7;out->last_io_count=1;}
 }
#ifdef RISC_USB_MSC_DIAGNOSTICS_V2
 if(test_case==26 || test_case==29 || test_case==30){
  assert(capacity>=sizeof(risc_usb_device_msc_diagnostics_v2));
  risc_usb_device_msc_diagnostics_v2 value={.base=*out,.transferred_bytes=512,.data_opcode=0x28,.data_lba=7,.data_requested=512,.data_transferred=512,.data_elapsed_ms=31};
  if(test_case==29){
   value.last_sense_key=value.last_sense_asc=value.last_sense_ascq=value.sense_opcode=value.sense_lba=UINT32_MAX;
   value.timeouts=value.last_timeout=value.aborts=value.last_abort=value.resets=value.unconfigures=value.suspends=value.resumes=value.start_stop_flags=value.eject_requested=value.eject_complete=UINT32_MAX;
   value.data_opcode=value.data_lba=value.data_requested=value.data_transferred=value.data_residue=UINT32_MAX;
   value.timeout_ms=value.data_elapsed_ms=UINT64_MAX;
   value.base.flags=value.transferred_bytes=value.residue=UINT32_MAX;
   value.base.blocks_written=value.base.max_poll_gap_ms=UINT64_MAX;
   if(configured)value.base.commands_started=UINT64_MAX;
   if(configured && steps>=12)value.base.commands_completed=value.base.blocks_read=UINT64_MAX;
  }
  value.base.struct_size=sizeof(value);memcpy(out,&value,sizeof(value));
 }
#else
 (void)capacity;
#endif
 return RISC_USB_MSC_OK;
}
static risc_usb_device_msc_api_v1_diagnostics selected_protocol;
#endif

static bool nav_poll(void*c,risc_input_navigation_frame_v1*out){
 (void)c;*out=(risc_input_navigation_frame_v1){0};
 if(nav_step==steps)return true;
 nav_step=steps;
 if(test_case!=6&&test_case!=11&&(test_case<13||test_case==17)&&(steps==8||steps==12))out->pressed=out->released=RISC_NAV_HOME;
 if(test_case!=6&&test_case!=11&&(test_case<13||test_case==17)&&steps==10)out->pressed=out->released=RISC_NAV_BACK;
 if((test_case==2||test_case==3||test_case==4)&&steps==21)out->pressed=out->released=RISC_NAV_CONFIRM;
 if(steps==80)out->pressed=out->released=RISC_NAV_HOME;
 return true;
}
static bool nav_foreground(void*c,const risc_input_foreground_v1*x,size_t n){(void)c;(void)x;(void)n;return true;}
static bool nav_reset(void*c){(void)c;return true;}
static const risc_input_navigation_api_v1 nav={1,sizeof(nav),NULL,nav_poll,nav_foreground,nav_reset};
static bool usb_touch_desired(void*c,risc_touch_snapshot_v1*out){
 (void)c;*out=(risc_touch_snapshot_v1){.width=480,.height=800};
 bool down=steps==2;unsigned y=644;
 /* Case17 injects its queued-frame Cancel in usb_touch_poll below. */
 if((test_case==0||test_case==10||test_case==11||test_case==12) && steps==20)down=true;
 if(test_case==6 && steps==20)down=true;
 if((test_case==6||test_case==7||test_case==11||test_case==12) && steps==40)down=true;
 if(test_case==2||test_case==3||test_case==4) {
  if(steps==20||steps==40||steps==50||(test_case==4&&steps==65))down=true;
  if(steps==30){down=true;y=735;}
 }
 if(down){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=240,.y=y};}
 return true;
}
/* Raw fixture obeys the ordered event/snapshot contract. */
static risc_touch_snapshot_v1 usb_touch_state={.width=480,.height=800};
static risc_touch_event_v1 usb_touch_events[32];
static unsigned usb_touch_head,usb_touch_tail,cancel_reports;
static bool usb_touch_poll(void*c,size_t n) {
 poll_touch(c,n);risc_touch_snapshot_v1 next={0};usb_touch_desired(c,&next);
 /* DOWN before readiness, then UP at/before its completion boundary.
  * The unchanged case17 assertion requires zero SD prepare calls. */
 if(test_case==17 && preparing_frames && cancel_reports<2) {
  assert(ms<=queued_until);next.contact_count=cancel_reports++==0;
  next.contacts[0]=(risc_touch_contact_v1){.id=1,.x=240,.y=644};
 }
 bool was=usb_touch_state.contact_count!=0,down=next.contact_count!=0;
 if(was!=down) {
  assert(usb_touch_tail-usb_touch_head<32);
  risc_touch_contact_v1 contact=down?next.contacts[0]:usb_touch_state.contacts[0];
  risc_touch_event_v1 event={.sequence=usb_touch_state.sequence+1,.timestamp_ms=ms,
   .kind=down?RISC_TOUCH_EVENT_DOWN:RISC_TOUCH_EVENT_UP,.id=contact.id,.x=contact.x,.y=contact.y};
  usb_touch_events[usb_touch_tail++%32]=event;
  next.sequence=event.sequence;
 }else next.sequence=usb_touch_state.sequence;
 next.timestamp_ms=ms;usb_touch_state=next;return true;
}
static int32_t usb_touch_next(void*c,uint64_t subscription,risc_touch_event_v1*out) {
 (void)c;assert(subscription==1);if(usb_touch_head==usb_touch_tail)return 0;
 *out=usb_touch_events[usb_touch_head++%32];return 1;
}
static bool usb_touch(void*c,risc_touch_snapshot_v1*out){(void)c;*out=usb_touch_state;return true;}
static bool usb_submit(void*c,risc_display_frame_v1 frame,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*key){
 (void)c;(void)r;(void)n;(void)o;assert(frame==1&&frames);frames=0;if(session&&preparing)preparing_frames++;*key=++presents;queued_until=ms+24;
 if(capture){char name[512];snprintf(name,sizeof(name),"%s/frame-%02u.pbm",capture,presents);FILE*f=fopen(name,"wb");assert(f);fprintf(f,"P4\n%u %u\n",PANEL_WIDTH,PANEL_HEIGHT);assert(fwrite(pixels,1,sizeof(pixels),f)==sizeof(pixels));fclose(f);}
 return true;
}
static bool usb_present(void*c,risc_display_present_token_v1 key,risc_display_present_status_v1*out){(void)c;(void)key;out->state=ms<queued_until?RISC_DISPLAY_PRESENT_QUEUED:RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static bool usb_health(risc_runtime_health_v1*out){out->uptime_ms=ms;return steps<100;}
static void usb_yield(uint32_t n){ms+=n;if(checkpoint_revoked){assert(!session&&!usb_releases&&!launches);longjmp(retained_exit,2);}if(test_case==8 && steps>=80){assert(session&&!usb_releases&&!ends&&!launches);longjmp(retained_exit,1);}assert(ms<30000);}
static bool usb_launch(const char*s){if(test_case==24)assert(steps<25);assert(!session&&!portable_usb_transfer_owned()&&usb_releases);return launch_app(s);}
static bool usb_release(risc_runtime_capability_v1*g){
 if(g->api==selected_api){assert(!session);
 if(test_case==30){assert(strstr(saved_checkpoint,"outcome=release-pending"));checkpoint_revoked=true;return false;}if(test_case==7&&!release_refused){release_refused=true;return false;}usb_releases++;}
 return release(g);
}
static bool usb_acquire(const char*name,uint32_t version,uint64_t id,risc_runtime_capability_v1*out){
 assert(version==1&&!id);
 if(!strcmp(name,"usb.device.msc")){out->api=selected_api;grants++;return true;}
 if(!strcmp(name,"input.navigation")){out->api=&nav;grants++;return true;}
 if(!strcmp(name,"board.battery"))return false;
 if(!acquire(name,version,id,out))return false;
 if(!strcmp(name,"input.touch.raw")){static risc_touch_api_v1 input;input=t;input.poll=usb_touch_poll;input.next=usb_touch_next;input.snapshot=usb_touch;out->api=&input;}
 if(!strcmp(name,"display.output")){static risc_display_output_api_v1 disp;disp=d;disp.submit=usb_submit;disp.present_status=usb_present;out->api=&disp;}
 return true;
}
static bool usb_diagnostic(const char*line){
 assert(strlen(line)<255);
 if(strstr(line,"USB transfer prepare step requested"))prepare_log_lines++;
 if(strstr(line,"USB command begin:")){command_begins++;assert(strstr(line,"name=READ(10)")&&strstr(line,"lba=7")&&strstr(line,"bytes=512"));if(test_case==23)assert(strstr(line,"count=3"));}
 if(strstr(line,"USB command end:")){command_ends++;assert(strstr(line,"result=passed")&&strstr(line,"elapsed_ms=31")&&strstr(line,"io_ms=12"));if(test_case==23)assert(strstr(line,"count=2"));}
 if(strstr(line,"host SD block access confirmed"))media_confirmations++;
 return diagnostic(line);
}
static risc_runtime_api_v1 usb_runtime={.api_version=1,.struct_size=sizeof(usb_runtime),.health=usb_health,.yield_ms=usb_yield,.diagnostic=usb_diagnostic,.request_launch=usb_launch,.acquire=usb_acquire,.release=usb_release,.diagnostic_checkpoint_client=checkpoint_get};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t version){return version==1?&usb_runtime:NULL;}
int main(int argc,char**argv){
 assert(argc>=2);test_case=(unsigned)atoi(argv[1]);if(test_case==27)usb_runtime.struct_size=offsetof(risc_runtime_api_v1,diagnostic_checkpoint_client);capture=argc>2?argv[2]:NULL;selected_msc=msc;selected_api=&selected_msc;
#ifdef RISC_USB_MSC_DIAGNOSTICS_TAG
 if(test_case>=18){
  selected_protocol=(risc_usb_device_msc_api_v1_diagnostics){.base=msc,.diagnostics_tag=RISC_USB_MSC_DIAGNOSTICS_TAG,.diagnostics_version=1,.diagnostics=usb_protocol};
  selected_protocol.base.base.struct_size=sizeof(selected_protocol);selected_api=&selected_protocol;
  if(test_case==19)selected_protocol.base.base.struct_size=sizeof(msc);
  if(test_case==20)selected_protocol.diagnostics_tag=0;
  if(test_case==21)selected_protocol.diagnostics_version=2;
  if(test_case==22)selected_protocol.diagnostics=NULL;
 }
#else
 assert(test_case<18);
#endif
 if(test_case==13)selected_msc.base.struct_size=sizeof(risc_usb_device_msc_api_v1);
 if(test_case==14)selected_msc.prepare_tag=0;
 if(test_case==15)selected_msc.prepare_version=2;
 if(test_case==16)selected_msc.prepare_step=NULL;
 if(setjmp(retained_exit)){
  if(test_case==30){assert(checkpoint_revoked && checkpoints==2 && strstr(saved_checkpoint,"outcome=release-pending"));puts("Pre-release checkpoint survives simulated Runtime authority revocation; no provider recallback or navigation");return 0;}
  puts("Retained tokenless begin: app and grant remain mapped, no navigation or cleanup");return 0;
 }
 assert(app_module_init()==0);app_main();app_module_fini();
 assert(!frames&&!grants&&!subs&&!session&&!portable_usb_transfer_owned());if(test_case<13||test_case>=17)assert(begins&&usb_steps);else assert(!begins&&!usb_steps&&!prepare_count&&usb_releases==1);
 assert(launches==1&&!strcmp(launched,"default.elf"));
 if(test_case==0)assert(ends==1&&cancel_ends==1&&!confirmed_ends);
 if(test_case==1||test_case==5||test_case==7)assert(ends==1&&!confirmed_ends);
 if(test_case==2||test_case==3)assert(cancel_ends==2&&confirmed_ends==1);
 if(test_case==3)assert(seen_states&(1u<<RISC_USB_MSC_SUSPENDED));
 if(test_case==4)assert(confirmed_ends==2&&retained_end);
 if(test_case==6)assert(begins==2&&usb_releases==2);
 if(test_case==7)assert(release_refused);
 if(test_case==9)assert(prepare_count==3&&ends==1&&!confirmed_ends);
 if(test_case==10)assert(prepare_count>1&&ends==1&&!configured&&!confirmed_ends);
 if(test_case==11)assert(begins==2&&prepare_count==5&&usb_releases==2&&ends==2&&!confirmed_ends);
 if(test_case==12)assert(prepare_count==2&&retained_end&&ends==2&&!confirmed_ends);
 if(test_case==17)assert(!prepare_count&&ends==1&&!confirmed_ends&&!configured);
 if(prepare_count)assert(preparing_frames&&preparing_wait_polls);
 assert(prepare_log_lines==prepare_count);
 if(test_case>=18){
  assert(ends==1&&!confirmed_ends&&media_confirmations==1);
  if(test_case==18||test_case==23||test_case>=24)assert(command_begins==1&&command_ends==1&&diagnostic_reads>2);
  else assert(!command_begins&&!command_ends&&!diagnostic_reads);
 }
 if(test_case==24)assert(steps<25);
 if(test_case>=25){
  if(test_case==27||test_case==28)assert(!checkpoints);
  else {assert(checkpoints==3 && strstr(saved_checkpoint,"outcome=complete"));
   if(test_case==25)assert(strstr(saved_checkpoint,"extended=0"));
   if(test_case==26)assert(strstr(saved_checkpoint,"extended=1")&&strstr(saved_checkpoint,"data_lba=7")&&strstr(saved_checkpoint,"data_transferred=512"));
   if(test_case==29)assert(checkpoint_requested>768 && strlen(saved_checkpoint)==768);
  }
 }
 printf("USB transfer case %u: %u owner polls, %u end calls, %u cable confirmations, clean Home\n",test_case,usb_steps,ends,confirmed_ends);
 return 0;
}
