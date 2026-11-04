#pragma once
/* Integration boundary to the verified shared background controller.
 * The implementation binds Context audio/RF, rules, presets and BLE; the root
 * owns OTA/Wi-Fi exclusivity, resident handoff and copied workflow payloads.
 */
#include "RiscRuntimeV1.h"
#include <stdbool.h>
bool ota_update_scene_background_init(const risc_runtime_api_v1 *,bool (*)(void *),void (*)(void *),void *);
bool ota_update_scene_background_capture(void);
bool ota_update_scene_background_step(bool settled);
bool ota_update_scene_background_pause(void);
bool ota_update_scene_background_close(void);

/* Checked copied configured timeout. False leaves automatic idle unavailable. */
bool ota_update_scene_background_idle_timeout(uint32_t *milliseconds);

bool ota_update_scene_background_capture_active(void);
uint32_t ota_update_scene_background_effects(void);
bool ota_update_scene_background_effect_done(unsigned effect,unsigned outcome);

/* Root callbacks. safe() must be pure. Pause successfully while safe BEFORE
 * entering Wi-Fi/OTA ownership, uncertain cleanup/bank, or restart pending.
 * Unsafe capture/step/pause never perform I/O and require previously paused
 * state; close and idle_timeout are deferred. Never mark unsafe ownership
 * cleared until the underlying provider has confirmed its release.
 * Workflow callbacks must copy payloads, never dispatch or reenter here.
 * Message acceptance queues a modal whose dismissal consumes the input.
 * Launch acceptance requires an installed, authorized route already checked.
 * Pending settings effects use the negotiated resident QUERY/APPLY/RESULT
 * protocol through files_scene_effects.h; false safe inhibits that protocol.
 */
bool ota_update_scene_background_safe(void);
bool ota_update_scene_background_message(const char text[33]);
bool ota_update_scene_background_launch(const char text[33]);
bool ota_update_scene_background_workflow_ready(void);
void ota_update_scene_background_idle_update(uint32_t milliseconds);
