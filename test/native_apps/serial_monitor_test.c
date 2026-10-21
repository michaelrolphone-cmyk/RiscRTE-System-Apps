/* Actual production UI + actual PortableSerialClient over bounded stream doubles.
 * Reuse the qualified client's fault vocabulary, not a replacement client. */
#define main serial_client_reference_tests
#include "portable_serial_client_test.c"
#undef main
#include "../../Apps/serial_monitor.c"
#include <stdlib.h>

#ifdef SM_TEST_INVALID_CATALOG
const portable_serial_monitor_instance portable_serial_monitor_instances[]={{0,"Unbound"},{42,"Second"}};
#else
const portable_serial_monitor_instance portable_serial_monitor_instances[]={{41,"First"},{42,"Second"},{43,"Third"},{44,"Fourth"},{45,"Fifth"},{46,"Sixth"},{47,"Seventh"},{48,"Eighth"}};
#endif
const unsigned portable_serial_monitor_instance_count=
#ifdef SM_TEST_INVALID_CATALOG
2;
#else
8;
#endif
static bool ui_retained;
static unsigned fences,ui_calls,poll_count,draws;
static uint32_t now;
static unsigned test_width=480,test_height=800;
static const char *scenario;
static void ui_enter(void){enter();++ui_calls;}
bool portable_adapter_retained(void){return ui_retained;}
static bool ui_retain(void){++fences;fenced=ui_retained=true;return true;}
void portable_adapter_retain_silent(void){if(!ui_retained)assert(runtime.retain_invocation());}
static int32_t ui_width(void){ui_enter();return (int32_t)test_width;}
static int32_t ui_height(void){ui_enter();return (int32_t)test_height;}
static void ui_clear(void){ui_enter();++draws;}
static void ui_rect(int32_t x,int32_t y,int32_t w,int32_t h,bool black){ui_enter();assert(x>=0 && y>=0 && x+w<=(int)test_width && y+h<=(int)test_height);(void)black;}
static void ui_present(bool full){ui_enter();(void)full;}
static void ui_back(bool enabled){ui_enter();assert(!enabled);}
static uint32_t ui_millis(void){ui_enter();return now;}
static bool ui_frame(void){ui_enter();return true;}
static void tap(t5_app_input_t *in,int x,int y){in->tapped=true;in->touch_x=x;in->touch_y=y;}
static bool ui_poll(t5_app_input_t *in,uint32_t wait) {
    ui_enter();assert(wait==20);assert(poll_count<100);now+=20;spent=0;
    unsigned step=poll_count++;
    if(!strcmp(scenario,"poll-failure") && step==5)return false;
    if(!strcmp(scenario,"pagination")) {
        if((step>=1 && step<=13 && (step&1)) || (step>=17 && step<=31 && (step&1)))tap(in,230,230);
        if(step==15 || step==33)tap(in,120,230);
        if(step==40)in->exit_requested=true;
        return true;
    }
    if(!strcmp(scenario,"held")) {
        if(step<=1 || step==3 || step==4 || step==7 || step==8)in->buttons=T5_APP_BUTTON_CONFIRM;
        if(step==6)in->buttons=T5_APP_BUTTON_DOWN;
        if(step==10)in->exit_requested=true;
        return true;
    }
    if(!strcmp(scenario,"no-selection") || !strcmp(scenario,"invalid-catalog")) {
        if(step==7)in->exit_requested=true;
        return true;
    }
    if(!strcmp(scenario,"repeat-tap")) {
        if(step==1 || step==2)tap(in,30,80);
        if(step==3)assert(!opens);
        if(step==4 || step==5)tap(in,30,104);
        if(step==7)in->exit_requested=true;
        return true;
    }
    if(step==1){if(!strcmp(scenario,"inventory-failure"))snapshot_ok=false;tap(in,30,80);} /* Instance 42, not registry-first 41. */
    if(!strcmp(scenario,"refresh-failure")) {
        if(step==3){snapshot_ok=false;tap(in,450,32);}
        if(step==5)tap(in,30,104);
        if(step==6)assert(!opens);
        if(step==7){snapshot_ok=true;tap(in,450,32);}
        if(step==9)tap(in,30,104);
        if(step==12)in->exit_requested=true;
        return true;
    }
    if(!strcmp(scenario,"back-before-open")) {
        if(step==3)tap(in,10,10);
        if(step==7)in->exit_requested=true;
        return true;
    }
    if(step==3) {
        if(!strcmp(scenario,"snapshot-binding"))devices[1].generation=888;
        if(!strcmp(scenario,"open-retained"))open_rc=PSC_RETAINED;
        if(!strcmp(scenario,"open-denied"))open_rc=PSC_BUSY;
        if(!strcmp(scenario,"open-malformed"))bad_open=2;
        tap(in,30,104); /* Exact second snapshot endpoint. */
    }
    if(step==4) {
        if(!strcmp(scenario,"call-retained"))call_rc=PSC_RETAINED;
        if(!strcmp(scenario,"read-retained"))read_rc=PSC_RETAINED;
        if(!strcmp(scenario,"read-malformed"))bad_count=1;
        if(!strcmp(scenario,"disconnect"))device_count=0;
        if(!strcmp(scenario,"close-retained"))close_rc=PSC_BUSY;
        if(!strcmp(scenario,"release-retained"))release_ok=false;
        if(!strcmp(scenario,"stale-invocation"))streams.context++;
    }
    if(!strcmp(scenario,"back-reselect") || !strcmp(scenario,"switch-instance")) {
        if(step==5)tap(in,10,10);
        if(step==7){if(!strcmp(scenario,"switch-instance")){expected_instance=41;tap(in,30,56);}else tap(in,30,80);}
        if(step==9)tap(in,30,104);
        if(step==11)in->exit_requested=true;
    } else if(step==7)in->exit_requested=true;
    return true;
}
static const t5_app_api_v1 ui_app={.abi_version=1,.struct_size=sizeof(ui_app),
    .screen_width=ui_width,.screen_height=ui_height,.clear=ui_clear,.fill_rect=ui_rect,
    .present=ui_present,.poll=ui_poll,.millis=ui_millis,.set_back_exits_app=ui_back,.frame_ready=ui_frame};
const t5_app_api_v1 *t5_app_get_api(uint32_t version){ui_enter();assert(version==1);return &ui_app;}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){ui_enter();assert(version==1);return &runtime;}
static void terminal_test(void) {
    sm_columns=8;sm_width=480;sm_height=160;sm_clear_terminal();
    const unsigned char bytes[]="abCD!\rX\nq\tR\b!\n\033\0\377";
    sm_receive(bytes,sizeof(bytes)-1);
    assert(!strcmp(sm_lines[0],"XbCD!"));assert(!strcmp(sm_lines[1],"q   !"));
    assert(!strcmp(sm_lines[2],"[1B][00]"));assert(!strcmp(sm_lines[3],"[FF]"));
    for(unsigned i=0;i<1000;i++)sm_receive((const unsigned char*)"01234567\n",9);
    assert(sm_count==SM_LINES && sm_head<SM_LINES && sm_column==0);
    sm_scroll=sm_max_scroll();assert(sm_scroll>0);
    for(unsigned i=0;i<100;i++)sm_receive((const unsigned char*)"tail\n",5);
    assert(sm_scroll<=sm_max_scroll());
    for(unsigned i=0;i<SM_LINES;i++)assert(strlen(sm_lines[i])<=sm_columns);
    sm_mode=SM_TERMINAL;sm_input_armed=true;
    t5_app_input_t in={0};tap(&in,10,150);sm_scroll=0;assert(!sm_event(&in));assert(sm_scroll==1);
    in=(t5_app_input_t){0};assert(!sm_event(&in));tap(&in,240,150);assert(!sm_event(&in));assert(!sm_scroll);
    sm_clear_terminal();assert(sm_count==1 && !sm_scroll && !sm_column && !sm_lines[0][0]);
    sm_receive((const unsigned char*)"abcde\rXY\b",9);
    assert(!strcmp(sm_lines[0],"X cde"));
    sm_receive((const unsigned char*)"z",1);assert(!strcmp(sm_lines[0],"Xzcde"));
    /* Every printable non-space character has a visible glyph, including
     * punctuation and case. ESC is represented as data, never interpreted. */
    for(unsigned c=1;c<95;c++){unsigned bits=0;for(unsigned y=0;y<12;y++)bits|=sm_font[c][y];assert(bits);}
    assert(memcmp(sm_font['A'-32],sm_font['a'-32],12));
    for(unsigned c=0;c<256;c++){unsigned char byte=(unsigned char)c;sm_receive(&byte,1);}
    for(unsigned i=0;i<SM_LINES;i++)for(const char *p=sm_lines[i];*p;p++)assert(*p>=32 && *p<127);
}
int main(int argc,char **argv) {
    assert(argc==2);scenario=argv[1];reset();runtime.retain_invocation=ui_retain;
    expected_instance=!strcmp(scenario,"pagination")?48:!strcmp(scenario,"held")?41:42;
    devices[0]=(risc_serial_device_v1){17,9,RISC_SERIAL_TRANSPORT_USB,{0}};
    devices[1]=(risc_serial_device_v1){23,11,RISC_SERIAL_TRANSPORT_USB,{0}};device_count=2;
    if(!strcmp(scenario,"pagination")) {
        device_count=8;test_width=test_height=240;
        for(unsigned i=0;i<8;i++)devices[i]=(risc_serial_device_v1){17+i,9+i,RISC_SERIAL_TRANSPORT_USB,{0}};
    }
    const char data[]="Hello, serial!\r\nCase & punctuation.";memcpy(echo,data,sizeof(data)-1);echo_size=sizeof(data)-1;rx_step=64;
    if(!strcmp(scenario,"terminal")){terminal_test();puts("serial monitor terminal PASS");return 0;}
    app_main();
    bool expect_retained=strstr(scenario,"retained") || !strcmp(scenario,"open-malformed") || !strcmp(scenario,"read-malformed") || !strcmp(scenario,"stale-invocation");
    if(expect_retained) {
        assert(ui_retained && fences==1 && !fenced_calls);
        unsigned calls_before=io_count(),ui_before=ui_calls;
        assert(!portable_serial_monitor_close());assert(!portable_app_before_launch("ignored.elf"));app_main();sm_render();
        assert(io_count()==calls_before && ui_calls==ui_before && fences==1);
    } else {
        assert(!ui_retained && !fences && !sm_client_ready && !sm_client.session && !sm_client.grant.api);
        bool no_open=!strcmp(scenario,"no-selection") || !strcmp(scenario,"invalid-catalog") || !strcmp(scenario,"back-before-open") || !strcmp(scenario,"inventory-failure");
        assert(opens==(no_open?0u:(!strcmp(scenario,"back-reselect") || !strcmp(scenario,"switch-instance"))?2u:1u));
        if(no_open){assert(!selected_device && !selected_generation);puts("serial monitor no unintended selection PASS");return 0;}
        assert(selected_device==(!strcmp(scenario,"pagination")?24u:!strcmp(scenario,"held")?17u:23u));
        assert(selected_generation==(!strcmp(scenario,"pagination")?16u:!strcmp(scenario,"held")?9u:11u));
        assert(!writes); /* Receive-only UI never toggles line state or sends. */
        assert(portable_serial_monitor_close());assert(portable_app_before_launch("ignored.elf"));
        if(!strcmp(scenario,"repeat")){poll_count=0;spent=0;app_main();assert(opens==2 && closes==2 && !sm_started);}
    }
    printf("serial monitor %s PASS (%u provider calls, %u UI calls)\n",scenario,io_count(),ui_calls);
    return 0;
}
