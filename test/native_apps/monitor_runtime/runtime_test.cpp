/* Production Runtime loader/graph/broker/queues, software-only provider modules.
 * No replacement Runtime API, capability acquire/release or stream functions. */
#include "bootstrap/Runtime.h"
#include "runtime/streams/ProviderQueueHost.h"
#include "RuntimeFileFixture.h"
#include "bridge_fixture.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
static std::string mode, output;
static uint32_t now;
static unsigned width, height, frames, pendingRx;
static bool pending, terminal;
static std::vector<std::string> events;
extern "C" const char* monitor_fixture_mode(void) { return mode.c_str(); }
extern "C" uint32_t monitor_fixture_now(void) { return now; }
extern "C" unsigned monitor_fixture_width(void) { return width; }
extern "C" unsigned monitor_fixture_height(void) { return height; }
extern "C" const char* monitor_fixture_output_dir(void) { return output.c_str(); }
extern "C" void monitor_fixture_advance(uint32_t ms) { assert(now + ms < 20000); now += ms; }
extern "C" void monitor_fixture_delay(uint32_t ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
extern "C" void monitor_fixture_present_pending(bool value) { assert(!terminal); pending = value; }
extern "C" void monitor_fixture_event(const char* value) {
    assert(value);
    const std::string e(value);
    if (terminal) assert(e == "app:returned");
    if (pending && e.find(":rx:") != std::string::npos) ++pendingRx;
    events.push_back(e);
    std::printf("EVENT %u %s\n", now, value);
    if (e.find(":terminal") != std::string::npos) terminal = true;
}
extern "C" void monitor_fixture_frame(const void* data, unsigned w, unsigned h, unsigned stride, uint64_t token) {
    assert(!terminal && data && w == width && h == height && stride >= w * 2);
    char path[1024];
    assert(std::snprintf(path, sizeof(path), "%s/frame-%03llu.ppm", output.c_str(), (unsigned long long)token) > 0);
    FILE* f = std::fopen(path, "wb"); assert(f);
    std::fprintf(f, "P6\n%u %u\n255\n", w, h);
    const auto* pixels = static_cast<const unsigned char*>(data);
    for (unsigned y=0; y<h; ++y) for (unsigned x=0; x<w; ++x) {
        uint16_t v; std::memcpy(&v, pixels+y*stride+x*2, 2);
        const unsigned char rgb[] = { (unsigned char)(((v>>11)&31)*255/31), (unsigned char)(((v>>5)&63)*255/63), (unsigned char)((v&31)*255/31) };
        assert(std::fwrite(rgb, 1, 3, f) == 3);
    }
    assert(!std::fclose(f)); ++frames;
}
static unsigned count(const std::string& e) { return unsigned(std::count(events.begin(), events.end(), e)); }
static unsigned bytes(const std::string& prefix) {
    unsigned total=0;
    for (const auto& e:events) if (e.rfind(prefix,0)==0) total += unsigned(std::stoul(e.substr(prefix.size())));
    return total;
}
static bool health(risc_runtime_health_v1* h) { h->uptime_ms=now; return true; }
static bool monotonic(uint32_t* t) { *t=now; return true; }
static bool logLine(const char* line) { std::printf("RUNTIME %s\n", line); return true; }
int main(int argc, char** argv) {
    assert(argc==7); const std::string root=argv[1]; mode=argv[2];
    const std::string activation=argv[3]; width=unsigned(std::stoul(argv[4])); height=unsigned(std::stoul(argv[5])); output=argv[6];
    assert(activation=="eager" || activation=="demand");
    const bool retained=mode=="close-retained" || mode=="call-overrun" || mode=="close-overrun";
    const bool denied=mode=="missing-grant" || mode=="wrong-grant";
    const unsigned chosen=mode=="select-second"?42:41;
    const unsigned sessions=denied?0:mode=="reopen"?2:1;
    const std::string serial="serial:"+std::to_string(chosen)+":";
    RiscTest::FileSources files({root});
    RiscBoot::Port port{[](){return true;},health,monitor_fixture_advance,logLine};
    port.monotonicMs=monotonic;
    auto* runtime=new RiscBoot::Runtime(port); assert(files.bind(*runtime));
    bool prepared=runtime->prepare(root.c_str());
    if(!prepared)std::fprintf(stderr,"prepare failed: %s\n",runtime->error());
    assert(prepared);
    const bool ran=runtime->run();
    if(ran!=!retained)std::fprintf(stderr,"run mismatch: %s\n",runtime->error());
    assert(ran==!retained && runtime->retained()==retained && terminal==retained);
    assert(count("app:init")==1 && count("app:initialized")==1 && count("app:main")==1 && count("app:returned")==1);
    assert(count(serial+"open")==sessions);
    assert(!count("serial:"+std::to_string(chosen==41?42:41)+":open"));
    for(const auto& e:events)assert(e.find(":raw:")==std::string::npos);
    const std::string other="serial:"+std::to_string(chosen==41?42:41)+":";
    assert(count(serial+"snapshot")==sessions);
    assert(!count(other+"snapshot"));
    const unsigned expectedStarts=activation=="eager"?1:sessions;
    assert(count(serial+"start")==expectedStarts);
    assert(count(other+"start")==unsigned(activation=="eager"));
    const auto initialized=std::find(events.begin(),events.end(),"app:init");
    for(const auto* name:{"display","touch","battery"}) {
        const std::string prefix=std::string(name)+".";
        assert(count(prefix+"start")==1);
        const auto start=std::find(events.begin(),events.end(),prefix+"start");
        assert((start<initialized)==(activation=="eager"));
        if(!retained) {
            assert(count(prefix+"quiesce")==1 && count(prefix+"stop")==1 && count(prefix+"unload")==1);
            assert(std::find(events.begin(),events.end(),prefix+"quiesce")<std::find(events.begin(),events.end(),prefix+"stop"));
            assert(std::find(events.begin(),events.end(),prefix+"stop")<std::find(events.begin(),events.end(),prefix+"unload"));
        }
    }
    const unsigned rx=bytes(serial+"rx:");
    if(denied)assert(!rx);
    else if(mode!="call-overrun")assert(rx==sessions*31);
    if(mode=="slow-frame")assert(pendingRx>0 && count("display.immutable-active")>0 && count("display.immutable-complete")>0);
    if(retained) {
        assert(!count("app:fini") && !count("app:unload"));
        assert(RuntimeStreams::Testing::allocatedBytes()==256);
        const auto boundary=std::find(events.begin(),events.end(),serial+"terminal"); assert(boundary!=events.end());
        assert(std::all_of(boundary+1,events.end(),[](const std::string& e){return e=="app:returned";}));
    } else {
        assert(count(serial+"close")==sessions);
        for(const auto& prefix:{serial,other}) {
            assert(count(prefix+"quiesce")==count(prefix+"start"));
            assert(count(prefix+"stop")==count(prefix+"start"));
            assert(count(prefix+"unload")==count(prefix+"start"));
        }
        assert(count("app:fini")==1 && count("app:finalized")==1 && count("app:unload")==1);
        assert(RuntimeStreams::Testing::allocatedBytes()==0);
        delete runtime;
    }
    const auto allocated=RuntimeStreams::Testing::allocatedBytes();
    std::ofstream result(output+"/result.json");
    result << "{\"mode\":\"" << mode << "\",\"activation\":\"" << activation << "\",\"width\":" << width << ",\"height\":" << height
           << ",\"opens\":" << sessions << ",\"rx_bytes\":" << rx << ",\"frames\":" << frames << ",\"rx_pending_frames\":" << pendingRx
           << ",\"queue_bytes_after_run\":" << allocated << ",\"retained\":" << (retained?"true":"false") << ",\"events\":[";
    for(size_t i=0;i<events.size();++i)result << (i?",":"") << '"' << events[i] << '"';
    result << "],\"result\":\"PASS\"}\n";result.close();assert(result);
    std::printf("monitor runtime %s/%s/%ux%u: PASS\n",mode.c_str(),activation.c_str(),width,height);std::fflush(stdout);
    // Retained Runtime is intentionally never destroyed, finalized or unloaded.
    // Process isolation is test disposal, not a recovery/teardown mechanism.
    if(retained)std::_Exit(0);
}
