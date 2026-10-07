#include "PortableTapSettings.h"
#include "PortableTapCalibration.h"
static portable_tap_settings tap_draft;
static portable_tap_calibration tap_calibration;
static twatch_tap_info_v1 tap_info;
static risc_runtime_capability_v1 tap_grant;
static const twatch_motion_api_v1 *tap_motion;
static const char *tap_message;
static bool tap_unconfirmed,tap_cleanup_pending,tap_requested_exit;
static unsigned tap_choice;
static bool settings_tap_close(void) {
  if(!tap_motion && !tap_grant.api){settings_motion_active=false;return true;}
  if(tap_cleanup_pending && (!tap_motion || !tap_motion->resume_wake(tap_motion->context))) {
    tap_message="Restore failed - retry";return false;
  }
  tap_cleanup_pending=false;
  if(tap_grant.api && !rt->release(&tap_grant)){tap_message="Release failed - retry";return false;}
  tap_grant=(risc_runtime_capability_v1){.struct_size=sizeof(tap_grant)};
  tap_motion=NULL;settings_motion_active=false;
#ifdef PORTABLE_APP_SLEEP_LOCAL
  last_activity=millis_now(); /* The measured interaction ends here. */
#endif
  return true;
}
