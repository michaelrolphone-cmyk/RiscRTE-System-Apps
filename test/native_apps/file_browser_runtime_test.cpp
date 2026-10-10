/* Production bootstrap Runtime + ProviderGraphV2 + mapped controller/provider.
 * Mirrors Runtime/test/run_file_open_test.sh without replacing Runtime.acquire. */
#include "bootstrap/Runtime.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace RiscBoot;
static std::string mode;
static std::vector<std::string> events;
static bool owned=true,recovered=false;
static unsigned visits;
static bool handoff() {return mode=="handoff" || mode=="nested-handoff";}
static bool nested() {return mode=="nested-no-handler" || mode=="nested-handoff";}
extern "C" const char *file_browser_runtime_mode(void) {return mode.c_str();}
extern "C" bool file_browser_runtime_fault(const char *name) {return !recovered && mode==name;}
extern "C" void file_browser_runtime_owner(bool value) {owned=value;}
extern "C" void file_browser_runtime_recover(void) {recovered=true;}
extern "C" unsigned file_browser_runtime_visit(void) {return ++visits;}
static unsigned count(const char *name) {return unsigned(std::count(events.begin(),events.end(),name));}
extern "C" void file_browser_runtime_event(const char *name) {
    if(!strcmp(name,"receiver"))assert(count("app-fini")==1 && count("app-return")==1);
    events.emplace_back(name);
}
static void save(const std::string &root,const char *name,const JsonDocument &doc) {
    std::ofstream out(root+"/"+name);std::string bytes;serializeJson(doc,bytes);out<<bytes;assert(out.good());
}
static void write(const std::string &root,const char *name,const char *bytes) {
    std::ofstream out(root+"/"+name);out<<bytes;assert(out.good());
}
static void provision(const std::string &root,const JsonDocument &selection) {
    write(root,"board.json",R"({"schema":"riscrte.board-hardware","schema_version":1,"board_id":"files-fixture","revision":"unspecified","buses":[],"devices":[{"instance_id":9,"chip":{"vendor":"fixture","model":"sd","revision":"unspecified"},"compatible":"fixture,sd","config_type":"gpio.bank","config_version":1,"config":{"pins":[5],"active_high":true,"pull_up":false,"debounce_us":0,"long_press_us":0,"click_min_us":0}}]})");
    write(root,"volume.json",R"({"type":"driver","id":"files-runtime-volume","version":"1.0.0","driver_abi":2,"architecture":"xtensa-esp32s3","file_name":"volume.elf","requires":[{"capability":"hardware.device","api":1}],"provides":[{"capability":"storage.volume","api":1}],"hardware_compatibility":[{"compatible":"fixture,sd","revisions":["unspecified"],"config_type":"gpio.bank","config_version":1}]})");
    JsonDocument app;app["type"]="application";app["id"]="file_browser";app["version"]="1.0.0";
    app["architecture"]="xtensa-esp32s3";app["file_name"]="browser.elf";app["entry"]="app_main";
    app["requires"].set(selection["requires"]);
    JsonDocument boot;boot["board"]="board.json";boot["default_app"]="browser.elf";boot["provider_activation"]="demand";
    auto drivers=boot["drivers"].to<JsonArray>();
    if(mode!="missing-provider") {auto driver=drivers.add<JsonObject>();driver["manifest"]="volume.json";driver["instance_id"]=9;}
    auto policies=boot["app_capabilities"].to<JsonArray>();auto policy=policies.add<JsonObject>();policy["manifest"]="browser.json";
    policy["grants"].set(selection["grants"]);
    if(mode=="wrong-grant")policy["grants"][0]["instance_id"]=8;
    if(mode=="installed-nine") {app["requires"][0]["capability"]="storage.installed-files";policy["grants"][0]["capability"]="storage.installed-files";}
    if(mode=="missing-grant")policy["grants"].as<JsonArray>().remove(0);
    if(handoff()) {
        write(root,"receiver.json",R"({"type":"application","id":"receiver","display_name":"Fixture Text Receiver","version":"1.0.0","architecture":"xtensa-esp32s3","file_name":"receiver.elf","entry":"app_main","requires":[{"capability":"file.open","api":1}],"supported_file_types":[".txt"]})");
        auto receiver=policies.add<JsonObject>();receiver["manifest"]="receiver.json";
        auto grant=receiver["grants"].to<JsonArray>().add<JsonObject>();grant["capability"]="file.open";grant["api"]=1;grant["instance_id"]=0;
    }
    save(root,"browser.json",app);save(root,"boot.json",boot);
}
int main(int argc,char **argv) {
    assert(argc==3);const std::string root=argv[1];JsonDocument selection;assert(readJson(argv[2],selection));
    for(const char *scenario:{"no-handler","handoff","nested-no-handler","nested-handoff","absent","refresh-error","listing-error","dir-close","file-close",
                             "wrong-instance","wrong-capability","wrong-grant","installed-nine","missing-provider","missing-grant"}) {
        mode=scenario;events.clear();visits=0;owned=true;recovered=false;
        const char *source=mode=="wrong-instance"?"browser-wrong-instance.elf":mode=="wrong-capability"?"browser-wrong-capability.elf":"browser-selected.elf";
        std::filesystem::copy_file(root+"/"+source,root+"/browser.elf",std::filesystem::copy_options::overwrite_existing);
        provision(root,selection);
        Runtime runtime({[](){return owned;},[](risc_runtime_health_v1*){return true;},[](uint32_t){},[](const char*){return true;}});
        const bool admissionFailure=mode=="wrong-grant" || mode=="installed-nine" || mode=="missing-provider" || mode=="missing-grant";
        const bool prepared=runtime.prepare(root.c_str());
        if(admissionFailure) {
            assert(!prepared && events.empty() && !visits);
            const char *expected=mode=="installed-nine"?"invalid installed-files authority":mode=="missing-grant"?"app requirement not uniquely authorized":"app provider unavailable";
            assert(!strcmp(runtime.error(),expected));
            std::printf("Files Runtime %s: rejected before execution (%s) PASS\n",scenario,runtime.error());continue;
        }
        if(!prepared)std::fprintf(stderr,"Unexpected admission error: %s\n",runtime.error());
        assert(prepared);const bool ran=runtime.run();
        if(!ran)std::fprintf(stderr,"Unexpected Runtime error: %s\n",runtime.error());
        assert(ran && !runtime.retained() && !risc_runtime_get_api(1));
        assert(visits==(handoff()?2u:1u));
        assert(count("app-init")==visits && count("app-fini")==visits && count("app-return")==visits);
        assert(count("provider-start")==count("provider-stop") && count("provider-start")==count("provider-quiesce"));
        assert(count("dir-open")==count("dir-close") && count("file-open")==count("file-close"));
        if(mode=="wrong-instance" || mode=="wrong-capability")assert(!count("provider-start") && !count("dir-open"));
        else assert(count("provider-start")>=1);
        assert(count("receiver")==unsigned(handoff()));
        assert(count("fresh-caller-return")==unsigned(handoff()));
        assert(count("no-handler")==unsigned(mode=="no-handler" || mode=="nested-no-handler"));
        assert(count("nested-dir-open")==unsigned(nested()));
        assert(count("nested-stat")==unsigned(nested()));
        assert(count("nested-file-open")==unsigned(nested()));
        assert(count("nested-preview")==unsigned(nested()));
        assert(count("cleanup-recovered")==unsigned(mode=="dir-close" || mode=="file-close"));
        std::printf("Files Runtime %s: controller entries=%u, provider starts/stops=%u/%u, directory opens/closes=%u/%u, file opens/closes=%u/%u PASS\n",
                    scenario,visits,count("provider-start"),count("provider-stop"),count("dir-open"),count("dir-close"),count("file-open"),count("file-close"));
    }
}
