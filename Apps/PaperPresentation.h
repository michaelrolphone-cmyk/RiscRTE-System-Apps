#pragma once
#include "SpringboardPresentation.h"
/* Private app/client contract. No provider or kernel ABI. Coordinates are
 * physical display pixels. Only a retaining monochrome portrait opts in. */
/* Text uses display typography; OR this flag into the text option for a
 * literal user-entry/key label, preserving ASCII letter case. */
#define PAPER_BUTTON_SLEEP_UNAVAILABLE (1u << 8)
#define PAPER_TEXT_LITERAL 256u
/* Private bitmap magnification for the main clock; measure remains unscaled. */
#define PAPER_TEXT_CLOCK 512u
#define PAPER_TEXT_LARGE 1024u
/* Fit complete labels horizontally, keeping their glyph height. */
#define PAPER_TEXT_FIT 2048u
typedef struct {
    uint32_t struct_size;
    void (*begin)(void);
    void (*text)(int x,int y,int width,const char *text,unsigned scale,bool heading,bool black);
    int (*measure)(const char *text,bool heading);
    void (*circle)(int x,int y,int radius,bool black);
    void (*contact)(springboard_contact *out);
    bool (*clock)(uint8_t *hour,uint8_t *minute);
    bool (*battery)(uint8_t *percent,bool *charging);
} paper_presentation;
const paper_presentation *paper_presentation_get(void);
