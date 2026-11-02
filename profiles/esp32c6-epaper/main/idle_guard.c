#include <stdbool.h>
#include "esp_freertos_hooks.h"
#include "idle_guard.h"

/* The byte-matched 1.1.1 reset PC is esp_cpu_wait_for_intr's WFI.
 * Keep the normal idle task and interrupt/task watchdogs running, but avoid
 * that wait instruction on this C6 board. This callback never blocks and
 * never feeds/disables a watchdog. Cost: increased CPU idle power.
 * Physical qualification is still required to establish the silicon cause. */
static bool keep_idle_responsive(void) { return false; }
esp_err_t biome_idle_guard_install(void) {
 return esp_register_freertos_idle_hook_for_cpu(keep_idle_responsive,0);
}
