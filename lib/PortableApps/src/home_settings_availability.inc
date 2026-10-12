/* Foreground status only. A read-only status grant never pins authority and
 * never closes anybody else's data grant or draft. Retained TIMER is excluded. */
#include "PortableSettingsAvailability.h"
static PortableSettingsAvailability home_settings_status;
static uint32_t home_settings_state=RISC_KEY_VALUE_UNAVAILABLE;
static uint64_t home_settings_generation;
bool portable_home_settings_poll(void) {
 if(native_custody_retained||failed)return false;
 if(!portable_settings_availability_open(&home_settings_status,rt,1)){portable_adapter_retain();return false;}
 uint32_t state=home_settings_status.state.availability;uint64_t generation=home_settings_status.state.generation;
 if(generation<home_settings_generation||(generation==home_settings_generation&&home_settings_generation&&state!=home_settings_state)){
  portable_adapter_retain();return false;
 }
 if(!portable_settings_availability_close(&home_settings_status,rt)){portable_adapter_retain();return false;}
 if(native_custody_retained||failed)return false;
 home_settings_state=state;home_settings_generation=generation;return true;
}
uint64_t portable_home_settings_generation(void) {return home_settings_generation;}
bool portable_home_settings_available(void) {return home_settings_state==RISC_KEY_VALUE_AVAILABLE;}
const char *portable_home_settings_notice(void) {
 return home_settings_state==RISC_KEY_VALUE_AVAILABLE?"":
  home_settings_state==RISC_KEY_VALUE_RECONCILIATION_PENDING?"SETTINGS PENDING; RESTART RUNTIME":
  home_settings_state==RISC_KEY_VALUE_CONFLICT?"SETTINGS CONFLICT; SOURCES KEPT":
  "SETTINGS UNAVAILABLE; RAM DEFAULTS";
}
static void home_settings_readonly_controls(void) {
 if(portable_home_settings_available())return;
 quick.ui.brightness_valid=quick.ui.volume_valid=quick.ui.tone_valid=false;
 quick.ui.dnd_valid=quick.ui.radios_valid=quick.ui.contexts_valid=false;
 quick.ui.error_flags|=PQA_ERROR_SAVE;
}

/* Read-only controls never accept a draft or a physical preview. Navigation,
 * USB handoff, clean refresh and temporary torch keep their existing routes. */
static uint32_t home_settings_filter_actions(uint32_t actions) {
 if(portable_home_settings_available())return actions;
 const uint32_t writes=PQA_BRIGHTNESS_PREVIEW|PQA_BRIGHTNESS_COMMIT|PQA_VOLUME_COMMIT|
  PQA_SILENT|PQA_WIFI|PQA_AIRPLANE|PQA_BLUETOOTH|PQA_DND|PQA_CONTEXTS|PQA_TONE_PREVIEW|PQA_TONE_COMMIT;
 if(actions&writes){quick.ui.error_flags|=PQA_ERROR_SAVE;home_settings_readonly_controls();}
 return actions&~writes;
}
