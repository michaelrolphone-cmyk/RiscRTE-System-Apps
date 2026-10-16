/* Real board RTC provider, with only the physical I2C/GPIO boundary modeled.
 * Compile this translation unit separately from the consumer-side ABI copy. */
#include <assert.h>
#include <string.h>
#define risc_runtime_get_api rtc_first_provider_runtime_get_api
#include RTC_PRODUCTION_SOURCE
#ifdef RTC_PCF85063
/* Provider logging is independent of the Settings UI diagnostic contract. */
const risc_runtime_api_v1 *rtc_first_provider_runtime_get_api(uint32_t v) {(void)v;return NULL;}
#endif

static uint8_t model_registers[256];
static unsigned calendar_writes, calendar_reads, read_failures, read_skip;
static bool claimed, write_failure;
#ifdef RTC_PCF85063
#define FIRST_TIME_REGISTER 4u
#define RTC_COMPATIBLE "nxp,pcf85063a"
#else
#define FIRST_TIME_REGISTER 2u
#define RTC_COMPATIBLE "nxp,pcf8563"
#endif
static bool model_claim(void *c,uint8_t address,uint64_t *out) {
    (void)c;assert(address==0x51 && !claimed);claimed=true;*out=73;return true;
}
static bool model_release(void *c,uint64_t token) {
    (void)c;assert(claimed && token==73);claimed=false;return true;
}
static bool model_transfer(void *c,uint64_t token,const uint8_t *tx,size_t n,
                           uint8_t *rx,size_t count,uint32_t timeout) {
    (void)c;assert(claimed && token==73 && tx && n && timeout==30);
    assert((size_t)tx[0]+(count?count:n-1)<=sizeof(model_registers));
    if(count) {
        assert(n==1 && rx);
        if(tx[0]==FIRST_TIME_REGISTER && count==7) {
            calendar_reads++;
            if(read_skip)read_skip--;
            else if(read_failures){read_failures--;return false;}
        }
        memcpy(rx,model_registers+tx[0],count);
    } else {
        if(tx[0]==FIRST_TIME_REGISTER && n==8) {
            calendar_writes++;
            if(write_failure)return false;
        }
        memcpy(model_registers+tx[0],tx+1,n-1);
    }
    return true;
}
static risc_i2c_bus_api_v1 model_bus={1,sizeof(model_bus),NULL,model_claim,model_transfer,model_release};
static tw_hw_i2c_device_v1 model_config;
static risc_hardware_device_v1 model_hardware;
static risc_provider_dependency_v1 model_dependencies[2];

const twatch_rtc_api_v1 *rtc_first_open(const twatch_rtc_time_v1 *initial,unsigned invalid) {
    assert(!claimed && !started);
    memset(model_registers,0,sizeof(model_registers));
    calendar_writes=calendar_reads=read_failures=read_skip=0;write_failure=false;
    model_config=(tw_hw_i2c_device_v1){.struct_size=sizeof(model_config),
        .bus={.struct_size=sizeof(risc_hw_bus_v1),.kind=2,.instance_id=101,
              .controller=0,.frequency_hz=400000,.sda=15,.scl=14,.sclk=-1,.mosi=-1,.miso=-1},
        .address=0x51,.irq=-1};
    model_hardware=(risc_hardware_device_v1){.api_version=1,.struct_size=sizeof(model_hardware),
        .instance_id=4,.compatible=RTC_COMPATIBLE,.revision="unspecified",.config_type="peripheral.i2c",
        .config_version=1,.config=&model_config,.config_size=sizeof(model_config)};
    model_dependencies[0]=(risc_provider_dependency_v1){"hardware.device",1,&model_hardware};
    model_dependencies[1]=(risc_provider_dependency_v1){"i2c.bus",1,&model_bus};
    uint8_t *raw=model_registers+FIRST_TIME_REGISTER;
    /* Independent BCD encoder: do not use the production encoder under test. */
    unsigned fields[7]={initial->second,initial->minute,initial->hour,initial->day,
                        initial->weekday,initial->month,initial->year-2000};
    for(unsigned i=0;i<7;i++)raw[i]=(uint8_t)(i==4?fields[i]:(fields[i]/10)*16+fields[i]%10);
    if(invalid==1)raw[0]|=0x80; /* oscillator-stop/voltage-low */
    if(invalid==2)raw[1]=0x6a;  /* corrupt BCD */
    if(invalid==3)raw[5]=0;     /* invalid month */
    model_registers[0]=0x20;   /* start from stopped oscillator */
    const risc_driver_v2 *descriptor=t5_driver_get(2);
    assert(descriptor && descriptor->start(model_dependencies,2));
    assert(!(model_registers[0]&0x20));
    return descriptor->capability;
}
void rtc_first_fault(unsigned reads,bool write) {read_failures=reads;write_failure=write;}
void rtc_first_fault_after(unsigned skip,unsigned reads) {read_skip=skip;read_failures=reads;}
unsigned rtc_first_writes(void) {return calendar_writes;}
unsigned rtc_first_reads(void) {return calendar_reads;}
void rtc_first_close(void) {assert(t5_driver_get(2)->quiesce());assert(!claimed);}
