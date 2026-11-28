#pragma once
/* Integration boundary to the verified shared background controller.
 * The implementation binds Context audio/RF, rules, presets and BLE; the root
 * owns PortableTextInputPolicy/Handoff focus and copied workflow payloads.
 */
#include "RiscRuntimeV1.h"
#include <stdbool.h>
bool files_scene_background_init(const risc_runtime_api_v1 *,bool (*)(void *),void (*)(void *),void *);
bool files_scene_background_capture(void);
bool files_scene_background_step(bool settled);
bool files_scene_background_pause(void);
bool files_scene_background_close(void);

/* Checked copied configured timeout. False leaves automatic idle unavailable. */
bool files_scene_background_idle_timeout(uint32_t *milliseconds);

bool files_scene_background_capture_active(void);
uint32_t files_scene_background_effects(void);
bool files_scene_background_effect_done(unsigned effect,unsigned outcome);
