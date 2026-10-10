#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {bool modal,visible,neutral;unsigned position,brightness;bool clean,audio;} reference_ui;
typedef void (*reference_observe)(reference_ui*);
typedef bool (*reference_point)(unsigned,int*,int*);
void reference_hooks(reference_observe,reference_point,void (*)(void));
void reference_event(unsigned,unsigned);
const char *reference_mode(void);
