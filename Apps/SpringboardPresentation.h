#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Private shared-app/client contract. Linked into the ELF, never a kernel,
 * firmware or provider ABI. Legacy hosts use the app's null implementation. */
typedef struct {
    bool valid, down, began, released, cancelled, tap_eligible;
    int16_t x, y;
} springboard_contact;
enum {
    SPRINGBOARD_FONT_ORBITRON_14=0,
    SPRINGBOARD_FONT_RAJDHANI_15=1,
    SPRINGBOARD_FONT_RAJDHANI_12=2,
    SPRINGBOARD_FONT_RAJDHANI_11=3
};
enum {
    SPRINGBOARD_ALIGN_LEFT=0,
    SPRINGBOARD_ALIGN_CENTER=1,
    SPRINGBOARD_ALIGN_RIGHT=2
};
typedef struct {
    uint32_t struct_size;
    void (*begin)(void);
    void (*circle)(int x, int y, int radius, uint32_t rgb, uint8_t opacity);
    bool (*icon)(int x, int y, int size, const char *name, uint8_t opacity);
    void (*caption)(int y, const char *text, bool clock, uint32_t rgb);
    void (*contact)(springboard_contact *out);
    bool (*clock)(uint8_t *hour, uint8_t *minute);
    void (*rect)(int x, int y, int width, int height, uint32_t rgb, uint8_t opacity);
    void (*round_rect)(int x, int y, int width, int height, int radius, uint32_t rgb, uint8_t opacity);
    int (*text)(int x, int y, int width, const char *text, uint8_t face,
                uint8_t align, int8_t tracking_q4, uint32_t rgb, uint8_t opacity);
} springboard_presentation;
const springboard_presentation *springboard_presentation_get(void);
/* App-local optional redraw request; weak false in the shared app. */
bool springboard_transition_active(void);
