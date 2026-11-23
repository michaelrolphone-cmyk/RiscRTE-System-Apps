/* Exercise the real patched SH2/SHTP sources with a nonresponsive HAL.
 * Only discovery is skipped by supplying the already-established control channel.
 * Public product/calibration APIs and the real transport retry loop do the work.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sh2.c"

static uint32_t clock_us;
static unsigned opens,closes,reads,writes;
static int write_mode=1;
static int hal_open(sh2_Hal_t *self){(void)self;++opens;return 0;}
static void hal_close(sh2_Hal_t *self){(void)self;++closes;}
static int hal_read(sh2_Hal_t *self,uint8_t *buffer,unsigned len,uint32_t *timestamp){
    (void)self;(void)buffer;(void)len;++reads;
    clock_us+=1000U;*timestamp=clock_us;return 0;
}
static int hal_write(sh2_Hal_t *self,uint8_t *buffer,unsigned len){
    (void)self;(void)buffer;++writes;
    return write_mode>0?(int)len:write_mode;
}
static uint32_t hal_time(sh2_Hal_t *self){(void)self;return clock_us;}
static sh2_Hal_t hal={hal_open,hal_close,hal_read,hal_write,hal_time};

static void open_discovered(void){
    assert(sh2_open(&hal,0,0)==SH2_OK);
    /* Channel0 already exists in SHTP; this avoids mocking advertisements. */
    _sh2.controlChan=0;
}
static void no_reply(void){
    write_mode=1;open_discovered();
    sh2_ProductIds_t ids;memset(&ids,0,sizeof(ids));
    uint32_t start=clock_us;
    assert(sh2_getProdIds(&ids)==SH2_ERR_TIMEOUT);
    assert((uint32_t)(clock_us-start)>=2000000U);
    assert((uint32_t)(clock_us-start)<=2001000U);
    assert(writes>0&&reads>0);
    start=clock_us;assert(sh2_setCalConfig(SH2_CAL_ACCEL)==SH2_ERR_TIMEOUT);
    assert((uint32_t)(clock_us-start)>=2000000U);
    start=clock_us;assert(sh2_saveDcdNow()==SH2_ERR_TIMEOUT);
    assert((uint32_t)(clock_us-start)>=2000000U);
    sh2_close();
    puts("PASS real SH2 product/calibration/save missing replies reach the deadline");
}
static void write_stall(void){
    write_mode=0;open_discovered();
    sh2_ProductIds_t ids;memset(&ids,0,sizeof(ids));
    uint32_t start=clock_us;
    assert(sh2_getProdIds(&ids)==SH2_ERR_TIMEOUT);
    assert((uint32_t)(clock_us-start)>=2000000U);
    assert((uint32_t)(clock_us-start)<=2001000U);
    sh2_close();
    puts("PASS real SHTP repeated zero-byte HAL writes reach the deadline");
}
static void negative_write(void){
    write_mode=-99;open_discovered();
    sh2_ProductIds_t ids;memset(&ids,0,sizeof(ids));
    uint32_t start=clock_us;
    assert(sh2_getProdIds(&ids)==-99);
    assert(clock_us==start);
    sh2_close();
    puts("PASS real SHTP negative HAL error ends the operation immediately");
}
static void lifecycle(void){
    sh2_close();sh2_close();
    unsigned before=closes;
    for(unsigned i=0;i<12;++i){write_mode=1;open_discovered();sh2_close();sh2_close();}
    assert(closes==before+12);
    puts("PASS null close and repeated sessions release the actual SHTP slots");
}
int main(int argc,char **argv){
    if(argc>1&&strcmp(argv[1],"no-reply")==0){no_reply();return 0;}
    if(argc>1&&strcmp(argv[1],"write-stall")==0){write_stall();return 0;}
    no_reply();write_stall();negative_write();lifecycle();
    clock_us=UINT32_MAX-1000000U;no_reply();
    puts("PASS real SH2 deadline remains bounded across the 32-bit microsecond wrap");
    return 0;
}
