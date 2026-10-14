/* Explicit controllers on the deployed RGB adapter. Preserve its optional
 * service/canvas retention fence and its settled foreground lifecycle. */
static void rgb_atomic_dispatch(void) {
 if(risc_runtime_exit_requested_v1(rt))navigation_pending|=T5_APP_BUTTON_BACK;
}
#define native_custody_retained app_invocation_retained()
#define portable_adapter_retain optional_services_retain
#define input_dispatch rgb_atomic_dispatch
#define home_pending false
#define handoff_requested false
#include "quick_atomic.inc"
#ifdef PORTABLE_CONTEXTS_CLIENT
#define contexts_before_storage() portable_background_stop()
#define contexts_before_storage_read() portable_background_stop()
#endif
#ifdef PORTABLE_BLE_BROADCAST
#define broadcast_before_storage() portable_background_stop()
#define broadcast_before_storage_read() portable_background_stop()
#endif
#include "home_automatic.inc"
#undef native_custody_retained
#undef portable_adapter_retain
#undef input_dispatch
#undef home_pending
#undef handoff_requested
#ifdef PORTABLE_CONTEXTS_CLIENT
#undef contexts_before_storage
#undef contexts_before_storage_read
#endif
#ifdef PORTABLE_BLE_BROADCAST
#undef broadcast_before_storage
#undef broadcast_before_storage_read
#endif
static bool home_automatic_input_busy(void){return home_automatic.running||home_automatic.operation.token;}
static uint32_t rgb_quick_take_action(void){quick_atomic_show_queue_error();return quick_atomic_take_action();}
static bool rgb_atomic_finish(void) {
 if(!quick_atomic_guard(NULL))return false;
 return quick_atomic_close()&&home_automatic_close();
}

static bool rgb_atomic_save(void *context,const char *key,const void *value,uint32_t size,const uint8_t *restore) {
 bool saved=quick_atomic_save(context,key,value,size,restore);
 /* A neutral raw sample consumed during the copied wait already ends the
  * old contact. Preserve that boundary so the next distinct tap is not lost.
  * Held, cancelled and unobserved contacts still require a fresh neutral. */
 if(quick_atomic_guard(NULL)&&touch.neutral&&!touch.down)
  (void)pqa_input(&quick.ui,input_sampled_at,true,0,0,0,0,false);
 return saved;
}
