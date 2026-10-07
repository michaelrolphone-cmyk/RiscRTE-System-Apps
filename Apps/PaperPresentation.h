#pragma once
#include "SpringboardPresentation.h"
/* Private app/client contract. No provider or kernel ABI. Coordinates are
 * physical display pixels. Only a retaining monochrome portrait opts in. */
/* Text uses display typography; OR this flag into the text option for a
 * literal user-entry/key label, preserving ASCII letter case. */
#define PAPER_TEXT_LITERAL 256u
typedef struct {
    uint32_t struct_size;
    void (*begin)(void);
    void (*text)(int x,int y,int width,const char *text,unsigned scale,bool heading,bool black);
    int (*measure)(const char *text,bool heading);
    void (*circle)(int x,int y,int radius,bool black);
    void (*contact)(springboard_contact *out);
    bool (*clock)(uint8_t *hour,uint8_t *minute);
} paper_presentation;
const paper_presentation *paper_presentation_get(void);
