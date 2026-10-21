#include "bootstrap/Runtime.h"
#include "RuntimeFileFixture.h"
#include "mouse_runtime_fixture.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

static std::string mode,activation;
static std::thread::id owner;
static mouse_runtime_controls controls{};
static std::vector<std::string> events;
static bool on_owner() {return std::this_thread::get_id()==owner;}
extern "C" mouse_runtime_controls *mouse_runtime_control() {assert(on_owner());return &controls;}
extern "C" void mouse_runtime_event(const char *name) {assert(on_owner());events.emplace_back(name);}
extern "C" void mouse_runtime_lower_call(const char *name) {
 assert(on_owner());++controls.lower_calls;events.emplace_back(name);
}
extern "C" const char *mouse_runtime_mode() {assert(on_owner());return mode.c_str();}
extern "C" bool mouse_runtime_demand() {assert(on_owner());return activation=="demand";}
extern "C" bool mouse_runtime_test_only_poll(bool (*poll)(void *,size_t),void *context,size_t budget) {
 assert(on_owner() && poll && budget==1);++controls.polls;return poll(context,budget);
}
static unsigned count(const char *event) {return unsigned(std::count(events.begin(),events.end(),event));}
static void write(const std::string &root,const char *name,const std::string &bytes) {
 std::ofstream out(root+"/"+name);out<<bytes;assert(out.good());
}
int main(int argc,char **argv) {
 assert(argc==4);const std::string root=argv[1];mode=argv[2];activation=argv[3];
 assert((mode=="normal" || mode=="close-retained" || mode=="connected-close" || mode=="unregistered-clock") && (activation=="eager" || activation=="demand"));
 owner=std::this_thread::get_id();controls.present=true;controls.now=100;controls.device=100;
 write(root,"board.json",R"({"schema":"riscrte.board-hardware","schema_version":1,"board_id":"test","revision":"unspecified","buses":[],"devices":[]})");
 write(root,"hid.json",R"({"type":"driver","id":"mouse-test-hid","version":"1.0.0","driver_abi":2,"architecture":"xtensa-esp32s3","file_name":"hid.elf","requires":[],"provides":[{"capability":"usb.hid","api":1}]})");
 /* The runner copies the unchanged recovered mouse manifest to mouse/ and
  * compiles its unchanged source to the manifest's exact driver.elf path. */
 write(root,"app.json",R"({"type":"application","id":"mouse-consumer-test","version":"1.0.0","architecture":"xtensa-esp32s3","file_name":"default.elf","entry":"app_main","requires":[{"capability":"usb.hid.mouse","api":1}]})");
 write(root,"boot.json",std::string(R"({"board":"board.json","default_app":"default.elf","provider_activation":")")+activation+
  R"(","drivers":[{"manifest":"hid.json"},{"manifest":"mouse/manifest.json"}],"app_capabilities":[{"manifest":"app.json","grants":[{"capability":"usb.hid.mouse","api":1,"instance_id":0}]}]})");
 static uint32_t now=0;
 static RiscTest::FileSources files({root.c_str()});
 RiscBoot::Port port{on_owner,[](risc_runtime_health_v1 *h){h->uptime_ms=++now;return true;},
  [](uint32_t){assert(on_owner());},[](const char *line){std::fprintf(stderr,"%s\n",line);return true;}};
 if(mode!="unregistered-clock")port.bindPlatforms=[](RiscBoot::Runtime &r){
  // Existing port registration API, not an admission-policy bypass or product binding.
  return r.registerPlatform("platform.clock",1,RiscBoot::Runtime::Scope::Global,0,mouse_runtime_clock());
 };
 auto *runtime=new RiscBoot::Runtime(port);
 assert(files.bind(*runtime));
 if(mode=="unregistered-clock") {
  assert(!runtime->prepare(root.c_str()));
  assert(std::string(runtime->error())=="missing scoped trusted platform provider");
  assert(!controls.lower_calls && !count("hid:start") && !count("app:init"));
  delete runtime;
  std::printf("mouse Runtime unregistered-clock/%s: reserved platform admission rejects before driver start PASS\n",activation.c_str());
  return 0;
 }
 if(!runtime->prepare(root.c_str())){std::fprintf(stderr,"prepare: %s\n",runtime->error());return 1;}
 const bool retained=mode=="close-retained";
 const bool ran=runtime->run();
 if(ran==retained || runtime->retained()!=retained) {
  std::fprintf(stderr,"run=%d retained=%d expected-retained=%d: %s\n",ran,runtime->retained(),retained,runtime->error());return 1;
 }
 assert(count("hid:start")==1 && count("app:init")==1);
 assert(count("case:initial-baseline")==1 && count("case:copied-once")==1);
 assert(controls.opens==(mode=="normal"?2u:1u) && controls.closes==controls.opens && controls.last_close && controls.polls);
 if(retained) {
  assert(controls.raw==controls.last_close && !count("hid:quiesce") && !count("hid:stop"));
  if(activation=="demand")assert(!count("app:fini") && count("case:demand-helper-retained")==1);
  else assert(count("app:fini")==1 && count("case:eager-boot-grant-remains")==1);
  std::printf("mouse Runtime %s/%s: exact raw custody + dependency graph retained; %s PASS\n",mode.c_str(),activation.c_str(),
   activation=="demand"?"helper grant retained, invocation fenced":"helper closed; boot-grant final teardown retained");
  std::fflush(stdout);
  /* Intentionally retained graph/module state must not be destructed/retried.
   * Process isolation is test disposal only, never a product recovery action. */
  std::_Exit(0);
 }
 if(mode=="normal")assert(count("case:focus-discard")==1 && count("case:overflow-resnapshot")==1 &&
  count("case:disconnect-once")==1 && count("case:reconnect-session")==1);
 else assert(count("case:connected-close")==1);
 assert(!controls.raw && count("hid:stop")==1 && count("app:complete")==1 && count("app:fini")==1);
 delete runtime;
 std::printf("mouse Runtime %s/%s: copied event/lifecycle contract and lower-call isolation PASS\n",mode.c_str(),activation.c_str());
}
