/* Run our guard through the actual IDF5.4.2 idle/tick dispatcher, not a copied
 * model. The CI command compiles components/esp_system/freertos_hooks.c. */
#include <assert.h>
#include "esp_freertos_hooks.h"
#include "idle_guard.h"
void esp_vApplicationIdleHook(void);
void esp_vApplicationTickHook(void);
static int waits,ticks;
void esp_cpu_wait_for_intr(void){waits++;}
static void tick(void){ticks++;}
int main(void){
 esp_vApplicationIdleHook();assert(waits==1);
 assert(biome_idle_guard_install()==ESP_OK);
 assert(esp_register_freertos_tick_hook_for_cpu(tick,0)==ESP_OK);
 for(int n=0;n<100;n++){esp_vApplicationIdleHook();esp_vApplicationTickHook();}
 assert(waits==1);assert(ticks==100);
 return 0;
}
