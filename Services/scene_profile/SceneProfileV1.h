#pragma once
#include <stdint.h>
#define RISC_SCENE_PROFILE_CAPABILITY "ui.presentation-profile"
/* Presentation policy only: this structure never crosses the application API.
 * Geometry is logical, before display rotation. Input rotation maps the raw
 * touch domain into that logical viewport independently of panel orientation. */
typedef struct {
    uint32_t api_version, struct_size, preferred_format, font_scale;
    uint32_t row_height, padding, foreground_rgb565, background_rgb565;
    uint32_t accent_rgb565, display_rotation, touch_rotation, reserved;
} risc_scene_profile_v1;
