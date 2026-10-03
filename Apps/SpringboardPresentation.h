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
typedef struct {
    uint32_t struct_size;
    void (*begin)(void);
    void (*circle)(int x, int y, int radius, uint32_t rgb, uint8_t opacity);
    bool (*icon)(int x, int y, int size, const char *name, uint8_t opacity);
    void (*caption)(int y, const char *text, bool clock, uint32_t rgb);
    void (*contact)(springboard_contact *out);
    bool (*clock)(uint8_t *hour, uint8_t *minute);
} springboard_presentation;
const springboard_presentation *springboard_presentation_get(void);
