#include "cst9217_test_model.h"
#include <stdio.h>
#define PORTABLE_NATIVE_CUSTODY_FENCE 1
static unsigned retains;
void portable_adapter_retain(void){++retains;}
#include "PortableTouch.h"
extern const risc_driver_v2 *t5_driver_get(uint32_t);
int main(void){
 const risc_driver_v2*d=t5_driver_get(2);const risc_touch_api_v1*a=d->capability;
 for(unsigned bad=0;bad<3;++bad)for(unsigned recover_first=0;recover_first<2;++recover_first){
  cst_test_model m;cst_test_setup(&m);m.touch=a;assert(d->start(m.dependencies,6));
  portable_touch t={.api=a,.subscription=a->subscribe(NULL)};assert(t.subscription);assert(portable_touch_sync(&t,true));retains=0;
  cst_test_report(&m,20,30,true);if(bad==0)m.raw[6]=0;if(bad==1)m.raw[5]=2;if(bad==2)cst_test_report(&m,466,30,true);
  assert(!portable_touch_collect(&t));assert(!retains&&!t.terminal);
  if(recover_first){cst_test_report(&m,0,0,false);assert(portable_touch_collect(&t));}
  portable_touch_sample out;(void)portable_touch_next(&t,&out);
  assert(out.cancelled);
#ifdef EXPECTED_REGRESSION
  assert((retains==1)==!recover_first);assert(t.terminal==!recover_first);
#else
  assert(!retains&&!t.terminal);
  if(!recover_first){
   for(unsigned n=0;n<8;++n){assert(!portable_touch_collect(&t));(void)portable_touch_next(&t,&out);assert(!t.terminal&&!retains&&out.cancelled);}
   cst_test_report(&m,20,30,true);assert(portable_touch_collect(&t));(void)portable_touch_next(&t,&out);assert(!t.terminal&&!t.neutral);
   cst_test_report(&m,0,0,false);assert(portable_touch_collect(&t));(void)portable_touch_next(&t,&out);assert(!out.tap_eligible||!out.released);
   (void)portable_touch_next(&t,&out);assert(t.neutral);
  }
#endif
  printf("PROVEN composition bad=%u recover-before-drain=%u terminal-retains=%u transfers-all-successful=1\n",bad,recover_first,retains);
  /* Owner fixture cleanup only, not a production app retry after retention. */
  assert(a->unsubscribe(NULL,t.subscription));assert(d->quiesce());cst_test_clean(&m);
 }
}
