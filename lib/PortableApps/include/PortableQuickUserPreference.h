#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Only explicit Quick controller actions receive this callback. Generic
 * preference setters used by automatic contexts/low-battery do not. The
 * optional restore byte belongs to brightness OFF, never another intent. */
typedef bool (*pqa_user_preference_save)(void *,const char *,const void *,uint32_t,
                                        const uint8_t *restore_brightness);
