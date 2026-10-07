#pragma once
#include "SpringboardPresentation.h"
/* Private app/client contract. No provider or kernel ABI. Coordinates are
 * physical display pixels. Only a retaining monochrome portrait opts in. */
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
