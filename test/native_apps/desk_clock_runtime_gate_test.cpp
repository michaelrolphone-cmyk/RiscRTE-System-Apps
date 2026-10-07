#include "ports/esp32s3/CpuPort.h"
#include "fixtures/desk_clock_runtime_gate.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
static RiscCpu::Port *cpu;
static RiscRetainedWake::Store *store;
static RiscRetainedWake::Image image{};
static risc_retained_wake_record_v1 expected{};
static int mode;
static unsigned reads,armed,cleaned;
static bool physicalHold,wakeArmed,timerArmed;
extern "C" int test_desk_gate_mode(void) { return mode; }
extern "C" void test_desk_gate_safe(bool safe) {
    assert(cpu->providerStorageSafe()==safe);
}
extern "C" void test_desk_gate_pending(const risc_retained_wake_record_v1 *record) {
    /* Test-only observation of pending bytes. Like Runtime's retained-wake
     * fixture, commit/rollback inspects RAM without changing pending staging.
     * App mutations above always go through the real Runtime capability. */
    assert(!image.magic);store->commit();
    if(record) {
        assert(image.magic && !memcmp(&image.record,record,sizeof(*record)));
        expected=*record;
    } else assert(!image.magic);
    store->rollback();
}
static bool owner() { return true; }
static uint64_t now() { return 0; }
static void waitMs(uint32_t) {}
static bool health(risc_runtime_health_v1 *health) { health->uptime_ms=0;return true; }
static bool logMessage(const char *text) { std::fprintf(stderr,"%s\n",text);return true; }
static bool gpioOpen(uint8_t,bool,bool,bool) { return true; }
static bool gpioWrite(uint8_t,bool) { return true; }
static bool gpioRead(uint8_t pin,bool *value) {
    assert(pin==7);++reads;
    *value=mode==2 || !timerArmed; /* Wake becomes active after admission/arm. */
    return true;
}
static bool gpioPwm(uint8_t,uint32_t,uint16_t,uint16_t) { return true; }
static bool close(uint8_t) { return true; }
static bool i2cOpen(uint8_t,uint8_t,uint8_t,uint32_t) { return true; }
static bool i2cTransfer(uint8_t,uint8_t,const uint8_t*,size_t,uint8_t*,size_t,uint32_t) { return true; }
static bool spiOpen(uint8_t,int16_t,int16_t,int16_t) { return true; }
static bool spiBegin(uint8_t,uint8_t,uint32_t,uint8_t,uint32_t) { return true; }
static bool spiTransfer(uint8_t,const uint8_t*,uint8_t*,size_t,uint32_t) { return true; }
static bool spiEnd(uint8_t,uint8_t,uint32_t) { return true; }
static bool valid(uint8_t pin) { return pin==7; }
static bool ready() { return true; }
static bool arm(uint8_t pin,bool active,bool pullup) {
    assert(pin==7 && !active && pullup && physicalHold);
    assert(!cpu->providerStorageSafe());wakeArmed=true;++armed;return true;
}
static bool clear(uint8_t pin,bool pullup) {
    assert(pin==7 && pullup && wakeArmed);wakeArmed=false;++cleaned;return true;
}
static bool timerArm(uint32_t milliseconds) {
    assert(milliseconds==59000 && wakeArmed);timerArmed=true;return true;
}
static bool timerClear() { assert(timerArmed);timerArmed=false;return true; }
static bool hold(uint8_t pin,bool enable) { assert(pin==6);physicalHold=enable;return true; }
static void enter() {
    assert(mode==2 && physicalHold && wakeArmed && timerArmed && reads==2);
    assert(!cpu->providerStorageSafe() && !image.magic);
    store->commit();
    assert(image.magic && image.checksum==RiscRetainedWake::checksum(image));
    assert(!std::memcmp(&image.record,&expected,sizeof(expected)));
    assert(!std::strcmp(image.identity.app,"desk-clock-gate-app"));
    std::puts("Real Runtime/CpuPort: pre-hold stage survives fenced prepared terminal entry PASS");
    std::fflush(stdout);std::_Exit(73); /* Hardware deep entry is terminal. */
}
static bool bind(RiscBoot::Runtime& runtime) { return cpu->bind(runtime); }
static bool appExitSafe() { return cpu->appExitSafe(); }
static bool storageSafe() { return cpu->providerStorageSafe(); }
static void file(const std::string& root,const char *name,const char *text) {
    std::ofstream output(root+"/"+name);output<<text;assert(output.good());
}
int main(int argc,char **argv) {
    assert(argc==3);const std::string root=argv[1];mode=std::atoi(argv[2]);assert(mode>=0 && mode<=2);
    file(root,"board.json",R"({"schema":"riscrte.board-hardware","schema_version":1,"board_id":"test","revision":"unspecified","buses":[],"devices":[{"instance_id":7,"chip":{"vendor":"test","model":"gpio","revision":"unspecified"},"compatible":"test,gpio","config_type":"gpio.bank","config_version":1,"config":{"pins":[7,6],"active_high":true,"pull_up":true,"debounce_us":0,"long_press_us":0,"click_min_us":0}}]})");
    file(root,"provider.json",R"({"type":"driver","id":"desk-clock-gate","version":"1.0.0","driver_abi":2,"architecture":"xtensa-esp32s3","file_name":"provider.elf","requires":[{"capability":"hardware.device","api":1},{"capability":"platform.gpio","api":1},{"capability":"platform.sync","api":1}],"provides":[{"capability":"test.desk-clock-gate","api":1}],"hardware_compatibility":[{"compatible":"test,gpio","revisions":["unspecified"],"config_type":"gpio.bank","config_version":1}]})");
    file(root,"app.json",R"({"type":"application","id":"desk-clock-gate-app","version":"1.0.0","architecture":"xtensa-esp32s3","file_name":"default.elf","entry":"app_main","requires":[{"capability":"test.desk-clock-gate","api":1},{"capability":"runtime.retained-wake","api":1}]})");
    file(root,"boot.json",R"({"board":"board.json","default_app":"default.elf","drivers":[{"manifest":"provider.json","instance_id":7}],"app_capabilities":[{"manifest":"app.json","grants":[{"capability":"test.desk-clock-gate","api":1,"instance_id":7},{"capability":"runtime.retained-wake","api":1,"instance_id":0}]}]})");
    file(root,"cohort.json",R"({"schema":"riscrte.cohort","schema_version":1,"product":"test","version":"1.0.0","runtime_version":"0.1.46","source_repo":"example/test","source_revision":"1111111111111111111111111111111111111111","layout":"riscrte-paired-16m-v1","store_abi":1,"firmware_size":32,"firmware_sha256":"1111111111111111111111111111111111111111111111111111111111111111"})");
    RiscRetainedWake::Store checkpoint(image);store=&checkpoint;store->boot(RISC_BOOT_POWER_ON);
    RiscCpu::Hardware hardware{owner,now,waitMs,gpioOpen,gpioWrite,gpioRead,gpioPwm,close,
        i2cOpen,i2cTransfer,close,spiOpen,spiBegin,spiTransfer,spiEnd,close};
    hardware.deepWakeValid=valid;hardware.deepReady=ready;hardware.deepWakeArm=arm;
    hardware.deepWakeClear=clear;hardware.deepSleep=enter;hardware.deepHold=hold;
    hardware.timerArm=timerArm;hardware.timerClear=timerClear;
    RiscCpu::Port port(hardware);cpu=&port;
    RiscBoot::Port boot{owner,health,waitMs,logMessage,bind,nullptr,appExitSafe,storageSafe};boot.retainedWake=store;
    RiscBoot::Runtime runtime(boot);assert(runtime.prepare(root.c_str()));assert(runtime.run());
    assert(mode!=2 && reads==2 && armed==1 && cleaned==1);
    assert(!physicalHold && !wakeArmed && !timerArmed && cpu->providerStorageSafe() && cpu->quiescent());
    test_desk_gate_pending(nullptr);
    std::puts(mode==1?"Real Runtime/CpuPort: catch-up replacement after full hold/lock restore PASS":
        "Real Runtime/CpuPort: held GPIO/lock stage+clear fences and refusal restore-before-clear PASS");
}
