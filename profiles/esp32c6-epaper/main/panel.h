#pragma once
#include "esp_err.h"
#include <stdint.h>
typedef struct { int busy,rst,dc,cs,mosi,sck; } panel_pins;
esp_err_t panel_init(panel_pins p);
esp_err_t panel_probe(void);
esp_err_t panel_render(const uint8_t *frame);
void panel_stop(void);
