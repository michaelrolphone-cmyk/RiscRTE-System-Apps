#include "bootstrap/Runtime.h"
#include "RiscDisplayOutputV1.h"
#include "RiscInputNavigationV1.h"
#include "RiscTouchV1.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <link.h>
#include <string>
#include <vector>
using RiscBoot::Runtime;
namespace {
struct Model {
    uint32_t clock=0, width=480, height=800, format=RISC_DISPLAY_FORMAT_MONO1;
    unsigned ready=0, polls=0, child=0, submit=0, releases=0, resets=0, waits=0;
    unsigned starts[3]{}, stops[3]{};
    bool held=false, foreground=false, input_failure=false, bad_info=false, short_api=false;
    bool acquire_failure=false, bad_surface=false, submit_failure=false, present_failure=false, never_complete=false;
    bool held_entry=false, sequence=false, touch_down=false, touch_gap=false, touch_sequence=false, touch_mismatch=false;
    unsigned navigation_case=0, foreground_rollbacks=0;
    bool touch_paging=false, foreground_failure=false, touch_only=false, zero_token=false;
    uint64_t subscription=0;
    std::vector<unsigned char> bytes, first_frame;
    std::vector<std::vector<unsigned char>> fresh_frames;
    std::vector<std::string> logs;
} m;
std::string root, previews;
static bool owner(){return true;}
static bool health(risc_runtime_health_v1 *h) {
    h->uptime_ms=m.clock;
    if(m.clock>25000) return false;
    if(m.touch_sequence || m.touch_paging || m.navigation_case==1) return m.ready < 2 || m.polls < 4;
    if(m.sequence) return m.ready < 4 || m.polls < 2;
    if(m.navigation_case) return m.polls < 8;
    return m.polls < (m.held_entry?12u:4u);
}
static void delay(uint32_t ms){assert(ms>=1 && ms<=50);m.clock+=ms;++m.waits;}
static bool log(const char *line){
    m.logs.emplace_back(line);
    if(!strncmp(line,"PAPER_SPACE ready",17)){++m.ready;m.polls=0;}
    return true;
}
static bool display_info(void*,risc_display_info_v1 *out){
    *out={};out->api_version=1;out->struct_size=sizeof(*out);out->width=m.width;out->height=m.height;
    out->supported_formats=RISC_DISPLAY_FORMAT_BIT(m.format);out->preferred_format=m.format;
    if(m.bad_info)out->width=5000;
    return true;
}
static bool acquire(void*,uint32_t format,risc_display_surface_v1 *out){
    assert(!m.held); if(m.acquire_failure)return false;
    assert(format==m.format);m.held=true;
    // Deliberately padded strides and an odd address catch packed/unaligned assumptions.
    uint32_t stride=(format==RISC_DISPLAY_FORMAT_MONO1?(m.width+7)/8:m.width*2)+3;
    m.bytes.assign(size_t(stride)*m.height+2,0xa5);
    *out={1,m.bytes.data()+1,m.width,m.height,stride,uint32_t(m.bytes.size()-2),format};
    if(m.bad_surface)out->size_bytes=1;
    return true;
}
static void release(void*,risc_display_frame_v1 frame){assert(frame==1 && m.held);m.held=false;++m.releases;}
static void guard(){assert(m.bytes.front()==0xa5 && m.bytes.back()==0xa5);}
static void write_preview(const std::string& name){
    if(previews.empty())return;
    std::ofstream f(previews+"/"+name+".pgm",std::ios::binary);
    f<<"P5\n"<<m.width<<" "<<m.height<<"\n255\n";
    uint32_t stride=(m.format==RISC_DISPLAY_FORMAT_MONO1?(m.width+7)/8:m.width*2)+3;
    for(uint32_t y=0;y<m.height;++y)for(uint32_t x=0;x<m.width;++x){
        auto *row=m.bytes.data()+1+size_t(y)*stride;
        unsigned char value=m.format==RISC_DISPLAY_FORMAT_MONO1?((row[x/8]&(0x80u>>(x&7)))?0:255):row[x*2];
        f.write(reinterpret_cast<const char*>(&value),1);
    }
}
static bool submit(void*,risc_display_frame_v1 frame,const risc_display_rect_v1*,size_t count,
                   const risc_display_present_options_v1 *options,risc_display_present_token_v1 *token){
    assert(frame==1 && m.held && count==0 && options->intent==RISC_DISPLAY_PRESENT_QUALITY);guard();
    if(m.submit_failure)return false;
    m.held=false;++m.submit;*token=m.zero_token?0:m.submit;
    if(m.polls==0)m.fresh_frames.emplace_back(m.bytes.begin()+1,m.bytes.end()-1);
    if(m.submit==1){m.first_frame=m.bytes;write_preview(std::string(m.format==RISC_DISPLAY_FORMAT_MONO1?"home-mono1-":"home-rgb565-")+std::to_string(m.width)+"x"+std::to_string(m.height));}
    return true;
}
static bool present_status(void*,risc_display_present_token_v1,risc_display_present_status_v1 *out){
    out->state=m.present_failure?RISC_DISPLAY_PRESENT_FAILED:m.never_complete?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;return true;
}
static bool wait_present(void *c,risc_display_present_token_v1 t,uint32_t,risc_display_present_status_v1 *out){return present_status(c,t,out);}
static risc_display_output_api_v1 display={1,sizeof(display),nullptr,display_info,acquire,release,submit,present_status,wait_present,nullptr};
static bool reset(void*){++m.resets;return true;}
static bool foreground(void*,const risc_input_foreground_v1 *claims,size_t count){
    assert(count<=1);if(count)assert(!strcmp(claims[0].capability,"input.touch.raw"));
    m.foreground=count!=0;if(!count)++m.foreground_rollbacks;
    return !(count && m.foreground_failure);
}
static bool nav_poll(void*,risc_input_navigation_frame_v1 *out){
    *out={};unsigned step=m.polls++;
    if(m.input_failure)return false;
    if(m.touch_sequence || m.touch_paging)return true;
    if(m.navigation_case && m.ready==1){
        if(m.navigation_case==1){
            if(step==1)out->buttons=out->pressed=RISC_NAV_UP;
            if(step==2)out->released=RISC_NAV_UP;
            if(step==3)out->buttons=out->pressed=RISC_NAV_DOWN;
            if(step==4)out->released=RISC_NAV_DOWN;
            if(step==5)out->buttons=out->pressed=RISC_NAV_CONFIRM;
            if(step==6)out->released=RISC_NAV_CONFIRM;
        }else if(m.navigation_case==2){
            if(step==1)out->buttons=out->pressed=RISC_NAV_CONFIRM;
            if(step==2)return false;
            if(step==3)out->released=RISC_NAV_CONFIRM;
        }else {
            if(step==1)out->buttons=out->pressed=RISC_NAV_CONFIRM|RISC_NAV_DOWN;
            if(step==2)out->released=RISC_NAV_CONFIRM|RISC_NAV_DOWN;
        }
        return true;
    }
    if(m.held_entry){
        if(step<3){out->buttons=RISC_NAV_CONFIRM;out->pressed=RISC_NAV_CONFIRM;}
        if(step==3)out->released=RISC_NAV_CONFIRM;
        return true;
    }
    if(!m.sequence || m.ready>=4)return true;
    // Each new default's target is initially its FIRST row: later targets
    // require actual new navigation, never retained selection/state.
    unsigned target=m.ready-1;
    if(step==0)return true;
    if(step<=target*2){if(step&1u)out->buttons=out->pressed=RISC_NAV_DOWN;else out->released=RISC_NAV_DOWN;return true;}
    if(step==target*2+1)out->buttons=out->pressed=RISC_NAV_CONFIRM;
    if(step==target*2+2)out->released=RISC_NAV_CONFIRM;
    return true;
}
static risc_input_navigation_api_v1 navigation={1,sizeof(navigation),nullptr,nav_poll,foreground,reset};
static uint64_t touch_subscribe(void*){assert(!m.subscription);return m.subscription=7;}
static bool touch_unsubscribe(void*,uint64_t id){assert(id==7);m.subscription=0;return true;}
static bool touch_poll(void*,size_t budget){assert(budget==1);if(m.touch_only)++m.polls;return true;}
static int32_t touch_next(void*,uint64_t,risc_touch_event_v1*){return m.touch_gap?-1:0;}
static bool touch_snapshot(void*,risc_touch_snapshot_v1 *out){
    *out={};out->width=m.width;out->height=m.touch_mismatch?m.height-1:m.height;
    if(m.touch_down || (m.touch_sequence && m.ready==1 && m.polls==2)){out->contact_count=1;out->contacts[0]={1,0,50,190};}
    if(m.touch_paging && m.ready==1 && (m.polls==2 || m.polls==4 || m.polls==6)){
        out->contact_count=1;out->contacts[0]={1,0,uint16_t(m.polls==6?40:200),218};
    }return true;
}
static risc_touch_api_v1 touch={1,sizeof(touch),nullptr,touch_subscribe,touch_unsubscribe,touch_poll,touch_next,touch_snapshot};
static void write(const std::string& path,const std::string& value){std::ofstream(root+"/"+path)<<value;}
static void configure(bool with_touch=false){
    write("board.json",R"({"schema":"riscrte.board-hardware","schema_version":1,"board_id":"host-paper-test","revision":"unspecified","buses":[],"devices":[]})");
    for(unsigned i=0;i<(with_touch?3u:2u);++i){
        if(i==1 && m.touch_only)continue;
        std::string id=i==0?"display":i==1?"navigation":"touch";
        std::string cap=i==0?"display.output":i==1?"input.navigation":"input.touch.raw";
        write(id+".json","{\"type\":\"driver\",\"id\":\""+id+"\",\"version\":\"1.0.0\",\"driver_abi\":2,\"architecture\":\"xtensa-esp32s3\",\"file_name\":\""+id+".elf\",\"requires\":[],\"provides\":[{\"capability\":\""+cap+"\",\"api\":1}]}");
    }
    std::string req=R"({"capability":"display.output","api":1})";
    if(!m.touch_only)req+=R"(,{"capability":"input.navigation","api":1})";
    std::string grants=R"({"capability":"display.output","api":1,"instance_id":0})";
    if(!m.touch_only)grants+=R"(,{"capability":"input.navigation","api":1,"instance_id":0})";
    if(with_touch){req+=R"(,{"capability":"input.touch.raw","api":1})";grants+=R"(,{"capability":"input.touch.raw","api":1,"instance_id":0})";}
    write("paper-space.json",R"({"type":"application","id":"paper-space","version":"1.0.0","architecture":"xtensa-esp32s3","file_name":"default.elf","entry":"app_main","requires":[)"+req+"]}");
    write("boot.json",R"({"board":"board.json","default_app":"default.elf","drivers":[{"manifest":"display.json"})"+std::string(m.touch_only?"":R"(,{"manifest":"navigation.json"})")+std::string(with_touch?R"(,{"manifest":"touch.json"})":"")+R"(],"app_capabilities":[{"manifest":"paper-space.json","grants":[)"+grants+"]}]}");
    write("corrupt.elf","not an ELF");
}
static bool run(bool with_touch=false){
    display.struct_size=m.short_api?8:sizeof(display);
    configure(with_touch);
    Runtime runtime({owner,health,delay,log});assert(runtime.prepare(root.c_str()));
    bool ok=runtime.run();assert(!risc_runtime_get_api(1));
    assert(!m.held && !m.foreground && !m.subscription);
    for(unsigned i=0;i<(with_touch?3u:2u);++i)assert(m.starts[i]==m.stops[i]);
    return ok;
}
}
extern "C" const void *paper_test_api(unsigned slot){return slot==0?static_cast<void*>(&display):slot==1?static_cast<void*>(&navigation):static_cast<void*>(&touch);}
extern "C" void paper_test_provider_start(unsigned slot){++m.starts[slot];}
extern "C" bool paper_test_provider_quiesce(unsigned slot){return slot==0?!m.held:slot==1?!m.foreground:!m.subscription;}
extern "C" void paper_test_provider_stop(unsigned slot){++m.stops[slot];}
extern "C" void paper_test_child(void){
    assert(!m.held && !m.foreground && !m.subscription);
    unsigned modules=0;
    dl_iterate_phdr([](dl_phdr_info *info,size_t,void *p){if(strstr(info->dlpi_name,"/tmp/riscrte-instance-"))++*static_cast<unsigned*>(p);return 0;},&modules);
    assert(modules==1); // The runtime maps only apps with unique-instance names; default is already unmapped.
    ++m.child;
}
int main(int argc,char **argv){
    assert(argc==3);root=argv[1];previews=argv[2];std::filesystem::create_directories(previews);
    for(uint32_t f:{RISC_DISPLAY_FORMAT_MONO1,RISC_DISPLAY_FORMAT_RGB565}){
        m={};m.format=f;m.sequence=true;assert(run());assert(m.ready==4 && m.child==1);
        assert(m.fresh_frames.size()==4);
        for(const auto &frame:m.fresh_frames)assert(frame==m.fresh_frames[0]);
        assert(std::count(m.logs.begin(),m.logs.end(),"RTE_APP child=failed action=reload-default")==2);
    }
    std::cout<<"Real Runtime: default -> child -> fresh default -> missing -> fresh default -> corrupt -> fresh default PASS\n";
    for(unsigned fault=0;fault<10;++fault){
        m={};switch(fault){case 0:m.bad_info=true;break;case 1:m.short_api=true;break;case 2:m.acquire_failure=true;break;
        case 3:m.bad_surface=true;break;case 4:m.submit_failure=true;break;case 5:m.present_failure=true;break;
        case 6:m.never_complete=true;break;case 7:m.input_failure=true;break;case 8:m.held_entry=true;break;case 9:m.zero_token=true;break;}
        bool ok=run();assert(ok==(fault>1));assert(m.child==0);assert(m.clock<=20040);
        if(fault==3 || fault==4)assert(m.releases==1);
        if(fault==6)assert(m.waits>=20000);
        if(fault==9)assert(m.releases==0 && m.submit==1);
    }
    // Separate actual touch build: held entry/gap cannot launch; unsubscribe
    // and foreground restoration precede grant teardown.
    std::filesystem::rename(root+"/default.elf",root+"/navigation-default.elf");
    std::filesystem::copy_file(root+"/both-default.elf",root+"/default.elf");
    for(bool gap:{false,true}){m={};m.touch_down=true;m.touch_gap=gap;assert(run(true));assert(m.child==0);}
    m={};m.touch_sequence=true;assert(run(true));assert(m.child==1 && m.ready==2);
    m={};m.touch_mismatch=true;assert(!run(true));assert(m.ready==0);
    m={};m.foreground_failure=true;assert(!run(true));assert(m.ready==0 && m.foreground_rollbacks==1);
    m={};m.width=m.height=240;m.touch_paging=true;assert(run(true));assert(m.ready==2 && m.child==0);
    assert(std::count(m.logs.begin(),m.logs.end(),"RTE_APP child=failed action=reload-default")==1);
    std::filesystem::copy_file(root+"/touch-default.elf",root+"/default.elf",std::filesystem::copy_options::overwrite_existing);
    m={};m.touch_only=m.touch_sequence=true;assert(run(true));assert(m.child==1 && m.ready==2 && m.starts[1]==0 && m.resets==0);
    m={};m.width=m.height=240;m.touch_only=m.touch_paging=true;assert(run(true));assert(m.ready==2 && m.child==0 && m.starts[1]==0);
    std::filesystem::remove(root+"/default.elf");
    std::filesystem::rename(root+"/navigation-default.elf",root+"/default.elf");
    for(unsigned nav_case:{1u,2u,3u}){m={};m.navigation_case=nav_case;assert(run());assert(m.child==(nav_case==1?1u:0u));}
    for(const auto &size:std::vector<std::pair<unsigned,unsigned>>{{240,240},{540,960},{960,540},{481,800}}){
        m={};m.width=size.first;m.height=size.second;assert(run());assert(m.submit==1);guard();
    }
    // Missing and corrupt default never spin/retry or call firmware GUI.
    std::filesystem::rename(root+"/default.elf",root+"/saved-default.elf");
    m={};assert(!run());assert(m.ready==0 && m.child==0);
    write("default.elf","corrupt default");m={};assert(!run());assert(m.ready==0);
    std::filesystem::remove(root+"/default.elf");std::filesystem::rename(root+"/saved-default.elf",root+"/default.elf");
    // Restart after every failure creates a new runtime/app invocation.
    m={};assert(run());assert(m.ready==1 && m.child==0 && m.resets==1);
    std::cout<<"Init/display/input failures, bounded present, held input/touch gap, cleanup, missing/corrupt default and restart PASS\n";
    // Final previews use the actual production example catalog, never the
    // missing/corrupt-child fault fixtures above.
    std::filesystem::copy_file(root+"/preview-default.elf",root+"/default.elf",std::filesystem::copy_options::overwrite_existing);
    for(uint32_t f:{RISC_DISPLAY_FORMAT_MONO1,RISC_DISPLAY_FORMAT_RGB565})
        for(const auto &size:std::vector<std::pair<unsigned,unsigned>>{{240,240},{480,800},{540,960},{960,540}}){
            m={};m.format=f;m.width=size.first;m.height=size.second;assert(run());assert(m.submit==1);
        }
}
