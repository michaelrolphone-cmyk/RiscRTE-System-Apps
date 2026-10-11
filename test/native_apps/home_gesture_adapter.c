/* Read-only observations of the production Quick Actions adapter. */
#include "../../lib/PortableApps/src/adapter.c"
bool home_gesture_quick_visible(void) {
#ifdef PORTABLE_QUICK_ACTIONS
 return quick_modal && pqa_visible(&quick.ui);
#else
 return false;
#endif
}
void home_gesture_finish(void) {handoff_requested=true;}
