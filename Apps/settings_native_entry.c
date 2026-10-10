/* Native-only completion boundary; preserve the portable controller verbatim. */
#include "PaperFrame.h"
#define app_main portable_settings_controller_main
#include "settings.c"
#undef app_main

__attribute__((visibility("default"))) void app_main(void) {
    portable_settings_controller_main();
    (void)paper_frame_drain();
}
