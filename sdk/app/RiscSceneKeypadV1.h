#pragma once
#include "RiscSceneComponentsV1.h"
/* An adjacent run of semantic keys forms a keypad. The presenter owns column
 * count, sizing, wrapping, focus, touch custody and native display geometry.
 * Each key has an independent stable node/action and a short label. There is
 * no app identity, arithmetic vocabulary, coordinate or device selector.
 * Older presenters reject the unfamiliar kind; callers must disclose that
 * capability gap rather than silently switch to app-owned rendering.
 * Existing scene/component table and document layouts remain byte-identical. */
#define RISC_COMPONENT_KEY (RISC_COMPONENT_CODE_ROW + 1u)
