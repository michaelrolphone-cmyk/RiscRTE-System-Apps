/* Production paper Clock/adapter over dimensioned providers.
 * Physical coordinates, presentation ownership, and persisted actions are observed.
 * No renderer/controller geometry is duplicated here. */
#define PAPER_QUICK_MAIN legacy_quick_main
#define PAPER_QUICK_RUNTIME legacy_quick_runtime
#include "paper_coordinate_quick_fixture.c"
#undef PAPER_QUICK_MAIN
#undef PAPER_QUICK_RUNTIME
static int tap_x,tap_y,expect_writes;static unsigned probe_mode;
static unsigned writes_while_down;
static bool delayed_pending;static unsigned delayed_until,delayed_checks;
static uint8_t immutable_pixels[sizeof(pixels)];
static bool coordinate_info(void*c,risc_display_info_v1*out){bool ok=qa_info(c,out);if(probe_mode==2)out->flags|=RISC_DISPLAY_INFO_ASYNC_PRESENT;return ok;}
static bool coordinate_frame(void*c,uint32_t f,risc_display_surface_v1*out){assert(!delayed_pending);return acquire_frame(c,f,out);}
static bool coordinate_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){
 assert(!delayed_pending);bool ok=qa_submit(c,f,r,n,o,t);if(ok&&probe_mode==2){memcpy(immutable_pixels,pixels,sizeof(pixels));delayed_pending=true;delayed_until=ms+75;}return ok;
}
static bool coordinate_status(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*out){
 if(probe_mode!=2)return present_status(c,t,out);
 assert(t==presents);assert(!memcmp(immutable_pixels,pixels,sizeof(pixels)));delayed_checks++;
 if(ms>=delayed_until)delayed_pending=false;
 out->state=delayed_pending?RISC_DISPLAY_PRESENT_QUEUED:RISC_DISPLAY_PRESENT_COMPLETE;return true;
}
static bool coordinate_touch(void*c,risc_touch_snapshot_v1*s){
 (void)c;*s=(risc_touch_snapshot_v1){.width=TEST_PAPER_LOGICAL_WIDTH,.height=TEST_PAPER_LOGICAL_HEIGHT};
 unsigned step=ms/25u;if(probe_mode==2)step=ms<250?0:ms<275?2:ms<300?3:ms<700?4:ms<725?6:ms<750?7:ms<1250?8:ms<1275?14:15;bool down=false;int x=TEST_PAPER_LOGICAL_WIDTH/2,y=0;
 if(step==2||step==3){down=true;y=(step==2?20:100)*TEST_PAPER_LOGICAL_HEIGHT/800;}
 if(step==6||step==7){down=true;x=tap_x;y=tap_y;if(writes)writes_while_down=writes;}
 
 if(step==14){down=true;y=675*TEST_PAPER_LOGICAL_HEIGHT/800;}
 if(down){s->contact_count=1;s->contacts[0]=(risc_touch_contact_v1){.id=1,.x=(uint16_t)x,.y=(uint16_t)y};}
#if defined(TEST_READER_FLIP) || defined(TEST_RAW_TOUCH_ROTATION)
 for(unsigned i=0;i<s->contact_count;i++){s->contacts[i].x=TEST_PAPER_LOGICAL_WIDTH-1-s->contacts[i].x;s->contacts[i].y=TEST_PAPER_LOGICAL_HEIGHT-1-s->contacts[i].y;}
#endif
 return true;
}
static bool coordinate_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){
 if(!qa_acquire(n,v,id,g))return false;
 if(!strcmp(n,"display.output")){static risc_display_output_api_v1 d;d=*(const risc_display_output_api_v1*)g->api;d.get_info=coordinate_info;d.acquire=coordinate_frame;d.submit=coordinate_submit;d.present_status=coordinate_status;g->api=&d;}
 if(!strcmp(n,"input.touch.raw")){qa_input_script=coordinate_touch;assert(qa_input_script(NULL,&qa_input_state));}
 return true;
}
static const risc_runtime_api_v1 coordinate_runtime={.api_version=1,.struct_size=sizeof(coordinate_runtime),.health=qa_health,.yield_ms=yield_ms,.diagnostic=diagnostic,.request_launch=launch_app,.acquire=coordinate_acquire,.release=release};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return v==1?&coordinate_runtime:NULL;}
int main(int argc,char**argv){
 assert(argc>=4);tap_x=atoi(argv[1]);tap_y=atoi(argv[2]);expect_writes=atoi(argv[3]);capture_dir=argc>4?argv[4]:NULL;probe_mode=argc>5?(unsigned)atoi(argv[5]):0;
 qa_case=probe_mode==1?14:6;assert(app_module_init()==0);app_main();app_module_fini();
 printf("provider=%dx%d logical=%dx%d tap=(%d,%d) writes=%u early=%u refreshes=%u presents=%u restore=%d frames=%u grants=%u subs=%u diagnostics=%u mode=%u\n",TEST_PAPER_WIDTH,TEST_PAPER_HEIGHT,TEST_PAPER_LOGICAL_WIDTH,TEST_PAPER_LOGICAL_HEIGHT,tap_x,tap_y,writes,writes_while_down,refreshes,presents,!memcmp(background,pixels,sizeof(pixels)),frames,grants,subs,diagnostics,probe_mode);
 assert(!frames&&!grants&&!subs&&!radio_acquires&&!launches&&!writes_while_down&&!delayed_pending);if(probe_mode==2){assert(delayed_checks);printf("Delayed immutable presentation checks=%u, no pending ownership\n",delayed_checks);}
 assert((int)writes==expect_writes);assert(!memcmp(background,pixels,sizeof(pixels)));return 0;
}
