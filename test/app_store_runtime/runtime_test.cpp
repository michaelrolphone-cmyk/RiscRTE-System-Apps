#include "bootstrap/Runtime.h"
#include "RuntimeFileFixture.h"
#include "RiscDisplayOutputV1.h"
#include "RiscTouchV1.h"
#include "RiscInputNavigationV1.h"
#include "RiscPlatformClockV1.h"
#include "PortableWifiCredentials.h"
#include "WifiApi.h"
#include "SoftwareUpdateV1.h"
#include "observer.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <dlfcn.h>
#include <map>
#include <string>
#include <vector>
static std::string mode;
static uint32_t width,height,format;
static uint64_t milliseconds,last_input;
static unsigned entries,loads,unloads,starts,stops,frames,io,nav_events,phase,begins,activations,cancels,connects,disconnects,writes;
static unsigned entry_begins;
static bool held,touch_sub,station_owned,cancel_delivered;
static wifi_link_t station=WIFI_LINK_DOWN;
static store_test_observer observer;
static const void *last_mapping;
static std::vector<uint8_t> pixels;
static std::map<std::string,std::vector<uint8_t>> values,before_values;
static software_update_status_v1 update_state={sizeof(update_state),SOFTWARE_UPDATE_LIST,1,0,0,0,false};
static void endpoint(){++io;}
static std::string key(uint32_t ns,const char*k){return std::to_string(ns)+":"+k;}
static int32_t kv_get(void*,uint32_t ns,const char*k,void*out,uint32_t cap,uint32_t*n){endpoint();assert(ns==1||ns==6);auto it=values.find(key(ns,k));if(it==values.end()){*n=0;return RISC_KEY_VALUE_NOT_FOUND;}*n=it->second.size();if(cap<*n)return RISC_KEY_VALUE_BUFFER_SMALL;std::memcpy(out,it->second.data(),*n);return 0;}
static int32_t kv_put(void*,uint32_t ns,const char*k,const void*b,uint32_t n){endpoint();assert(ns==6);writes++;values[key(ns,k)]=std::vector<uint8_t>((const uint8_t*)b,(const uint8_t*)b+n);return 0;}
static RiscBoot::KeyValueBackend keys{nullptr,kv_get,kv_put};
static int32_t seed_get(void*,const char*k,void*b,uint32_t cap,uint32_t*n){return kv_get(nullptr,6,k,b,cap,n);}
static int32_t seed_put(void*,const char*k,const void*b,uint32_t n){return kv_put(nullptr,6,k,b,n);}
extern "C" void store_test_mapping(unsigned event,store_test_observer fn){if(event==1){loads++;observer=fn;last_mapping=(const void*)fn;}else{assert(event==2&&observer==fn);unloads++;observer=nullptr;}}
extern "C" unsigned store_test_entry(){entries++;phase=0;last_input=milliseconds;entry_begins=begins;cancel_delivered=false;assert(!held&&!touch_sub&&!station_owned);update_state={sizeof(update_state),SOFTWARE_UPDATE_LIST,1,0,0,0,false};return entries;}
static bool info(void*,risc_display_info_v1*out){endpoint();*out={};out->api_version=1;out->struct_size=sizeof(*out);out->width=width;out->height=height;out->supported_formats=RISC_DISPLAY_FORMAT_BIT(format);out->preferred_format=format;return true;}
static bool acquire(void*,uint32_t f,risc_display_surface_v1*out){endpoint();assert(!held&&f==format);held=true;*out={frames+1,pixels.data(),width,height,format==1?(width+7)/8:width*2,(uint32_t)pixels.size(),format};return true;}
static void release(void*,uint64_t f){endpoint();assert(held&&f);held=false;}
static bool submit(void*,uint64_t f,const risc_display_rect_v1*,size_t n,const risc_display_present_options_v1*,uint64_t*out){endpoint();assert(held&&f&&!n);*out=++frames;held=false;return true;}
static bool present(void*,uint64_t f,risc_display_present_status_v1*out){endpoint();assert(f&&!held);out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static const risc_display_output_api_v1 display={1,sizeof(display),nullptr,info,acquire,release,submit,present,nullptr,nullptr};
static uint64_t subscribe(void*){endpoint();assert(!touch_sub);touch_sub=true;return 1;}
static bool unsubscribe(void*,uint64_t token){endpoint();assert(token==1&&touch_sub);touch_sub=false;return true;}
static bool touch_poll(void*,size_t){endpoint();return true;}
static int32_t next(void*,uint64_t token,risc_touch_event_v1*){endpoint();assert(token==1&&touch_sub);return 0;}
static bool snapshot(void*,risc_touch_snapshot_v1*out){endpoint();*out={};out->width=width;out->height=height;return true;}
static const risc_touch_api_v1 touch={1,sizeof(touch),nullptr,subscribe,unsubscribe,touch_poll,next,snapshot};
static unsigned focus_action(const store_test_view&v){for(unsigned i=0;i<v.document.node_count;i++)if(v.document.nodes[i].id==v.navigation.focus[0])return v.document.nodes[i].action;return 0;}
static bool navigation_poll(void*,risc_input_navigation_frame_v1*out){endpoint();*out={};if(!observer||milliseconds-last_input<80)return true;store_test_view v{};observer(&v);if(!v.session||!v.document.revision||(v.flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY|RISC_SCENE_CLOSING)))return true;
 bool confirming=(v.document.flags&RISC_COMPONENTS_CONFIRM)!=0;unsigned button=0,target=0;
 if(phase==0){bool details=false;for(unsigned i=0;i<v.document.node_count;i++)details|=v.document.nodes[i].action==10;if(confirming||details){phase=1;return true;}target=256;}
 else if(phase==1){if(begins>entry_begins){phase=2;return true;}bool list=false;for(unsigned i=0;i<v.document.node_count;i++)list|=v.document.nodes[i].action==256;if(list){phase=3;return true;}if(mode=="cancel"){button=RISC_NAV_BACK;}else target=(v.document.flags&RISC_COMPONENTS_CONFIRM)?5:10;}
 else if(phase==2){if(update_state.state==SOFTWARE_UPDATE_LIST&&!update_state.resources_open){phase=3;return true;}target=4;}
 else if(phase==3)button=RISC_NAV_HOME;
 if(target){bool present_target=false;for(unsigned i=0;i<v.document.node_count;i++)present_target|=v.document.nodes[i].action==target;if(!present_target)return true;button=focus_action(v)==target?RISC_NAV_CONFIRM:RISC_NAV_DOWN;if(phase==2&&button==RISC_NAV_CONFIRM)cancel_delivered=true;}
 if(button){out->pressed=out->released=button;last_input=milliseconds;nav_events++;}return true;}
static bool foreground(void*,const risc_input_foreground_v1*,size_t){endpoint();return true;}
static bool nav_reset(void*){endpoint();return true;}
static const risc_input_navigation_api_v1 navigation={1,sizeof(navigation),nullptr,navigation_poll,foreground,nav_reset};
static bool wifi_connect(void*,const char*s,const char*p){endpoint();assert(!held&&!touch_sub&&!station_owned&&!std::strcmp(s,"Saved network")&&!std::strcmp(p,"testpass"));connects++;station_owned=true;station=WIFI_LINK_UP;return true;}
static bool wifi_disconnect(void*){endpoint();assert(!held&&!touch_sub);disconnects++;station_owned=false;station=WIFI_LINK_DOWN;return true;}
static wifi_link_t wifi_status(void*){endpoint();return station;}
static int8_t rssi(void*){endpoint();return -40;}
static bool addresses(void*,wifi_ipv4_v1*s,wifi_ipv4_v1*a){endpoint();*s={{192,0,2,1},{},{}};*a={};return station==WIFI_LINK_UP;}
static bool scan_start(void*){assert(false);return false;}
static bool scan_poll(void*,garden_radio_scan_result_v1*){assert(false);return false;}
static bool scan_cancel(void*){assert(false);return false;}
static wifi_api_v1 wifi{};
static bool update_refresh(void*,uint64_t){endpoint();assert(false);return false;}
static bool update_step(void*){endpoint();if(update_state.state==SOFTWARE_UPDATE_PREPARING){update_state.state=SOFTWARE_UPDATE_VERIFYING;update_state.done=50;update_state.total=100;}if(cancel_delivered&&update_state.resources_open)update_state.state=SOFTWARE_UPDATE_READY;return true;}
static bool update_status(void*,software_update_status_v1*out){endpoint();assert(out->struct_size==sizeof(*out));*out=update_state;return true;}
static bool update_get(void*,uint32_t index,software_update_row_v1*out){endpoint();assert(index==0);*out={};out->struct_size=sizeof(*out);out->availability=SOFTWARE_UPDATE_AVAILABLE;out->size=1024;std::strcpy(out->id,"Example app");std::strcpy(out->version,"2.0.0");std::strcpy(out->installed,"1.0.0");std::strcpy(out->reason,"New release");return true;}
static bool update_begin(void*,uint32_t index,uint64_t utc){endpoint();assert(index==0&&utc==1800000000&&station_owned&&mode=="begin-cancel");begins++;update_state.state=SOFTWARE_UPDATE_PREPARING;update_state.resources_open=true;return true;}
static bool update_cancel(void*){endpoint();cancels++;update_state.state=SOFTWARE_UPDATE_LIST;update_state.resources_open=false;update_state.done=update_state.total=0;return true;}
static bool update_activate(void*){endpoint();activations++;assert(!"Cancel must be consumed before READY activation");return false;}
static bool update_restart(void*){endpoint();assert(false);return false;}
static const software_update_v1 update={1,sizeof(update),nullptr,update_refresh,update_step,update_status,update_get,update_begin,update_cancel,update_activate,update_restart};
extern "C" const void*store_test_provider(const char*n){if(!std::strcmp(n,"display.output"))return &display;if(!std::strcmp(n,"input.touch.raw"))return &touch;if(!std::strcmp(n,"input.navigation"))return &navigation;if(!std::strcmp(n,"net.wifi"))return &wifi;if(!std::strcmp(n,"software.update.apps"))return &update;assert(false);return nullptr;}
extern "C" bool store_test_provider_event(const char*n,unsigned event){if(event==1)starts++;else if(event==2)stops++;else if(event==3){if(!std::strcmp(n,"display.output"))return !held;if(!std::strcmp(n,"input.touch.raw"))return !touch_sub;if(!std::strcmp(n,"net.wifi"))return !station_owned;}return true;}
static uint64_t clock_ms(void*){return milliseconds;}
static void clock_sleep(void*,uint32_t){assert(false);}
static const risc_platform_clock_api_v1 clock_api={1,sizeof(clock_api),nullptr,clock_ms,clock_sleep};
static int32_t realtime_read(void*,risc_realtime_snapshot_v1*out){endpoint();*out={sizeof(*out),RISC_REALTIME_VALID,1800000000,0,0,milliseconds*1000,milliseconds*1000};return 0;}
static int32_t realtime_seed(void*,int64_t,uint32_t){assert(false);return -1;}
static const risc_realtime_control_api_v1 realtime={1,sizeof(realtime),nullptr,realtime_read,realtime_seed};
int main(int argc,char**argv){assert(argc==6);mode=argv[2];width=std::strtoul(argv[3],nullptr,10);height=std::strtoul(argv[4],nullptr,10);format=std::strtoul(argv[5],nullptr,10);pixels.resize(width*height*2);
 wifi.api_version=1;wifi.struct_size=sizeof(wifi);wifi.connect=wifi_connect;wifi.status=wifi_status;wifi.rssi=rssi;wifi.addresses=addresses;wifi.scan_start=scan_start;wifi.scan_poll=scan_poll;wifi.scan_cancel=scan_cancel;wifi.disconnect_checked=wifi_disconnect;
 risc_key_value_v1 seed={1,sizeof(seed),nullptr,seed_get,seed_put};portable_wifi_credentials credentials={"Saved network","testpass"};assert(portable_wifi_credentials_save(&seed,&credentials)==PORTABLE_WIFI_CREDENTIALS_SAVED);values["1:quick_radio"]={0x51,1,1,0xa4};before_values=values;writes=io=0;
 RiscBoot::Port port{[](){return true;},[](risc_runtime_health_v1*h){h->uptime_ms=milliseconds;return true;},[](uint32_t n){milliseconds+=n;if(milliseconds>120000){store_test_view v{};if(observer)observer(&v);std::fprintf(stderr,"Timeout entry%u phase%u frames%u nav%u begin%u state%u flags%u focus%u nodes%u\n",entries,phase,frames,nav_events,begins,update_state.state,v.flags,v.navigation.focus[0],v.document.node_count);std::abort();}},[](const char*){return true;},nullptr,&keys};
 port.monotonicMs=[](uint32_t*out){*out=(uint32_t)milliseconds;return true;};port.bindPlatforms=[](RiscBoot::Runtime&r){return r.registerPlatform("platform.clock",1,RiscBoot::Runtime::Scope::Global,0,&clock_api)&&r.registerRealtime(&realtime);};
 RiscBoot::Runtime runtime(port);RiscTest::FileSources files({std::string(argv[1])});assert(files.bind(runtime));if(!runtime.prepare(argv[1])){std::fprintf(stderr,"Admission: %s\n",runtime.error());return 1;}if(!runtime.run()){std::fprintf(stderr,"Runtime: %s\n",runtime.error());return 2;}
 assert(!runtime.retained()&&loads==2&&unloads==2&&entries==2&&!held&&!touch_sub&&!station_owned&&values==before_values&&!writes&&!activations);Dl_info mapping{};assert(!dladdr(last_mapping,&mapping));
 if(mode=="no-ui")assert(!io&&!frames&&!begins&&!connects&&!starts);else{assert(frames&&nav_events&&begins==(mode=="begin-cancel"?2u:0u));}
 std::printf("App Store real Runtime %s %ux%u format%u: frames=%u native-nav=%u begin=%u cancel=%u activation=0 writes=0 unloads=2 PASS\n",mode.c_str(),width,height,format,frames,nav_events,begins,cancels);
}
