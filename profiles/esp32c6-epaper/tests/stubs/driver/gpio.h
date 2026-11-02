#pragma once
#include <stdint.h>
#include "esp_err.h"
#define GPIO_MODE_OUTPUT 1
#define GPIO_MODE_INPUT 0
#define GPIO_PULLUP_ENABLE 1
#define GPIO_IS_VALID_GPIO(p) ((p)>=0&&(p)<=30)
#define GPIO_IS_VALID_OUTPUT_GPIO(p) GPIO_IS_VALID_GPIO(p)
typedef struct {uint64_t pin_bit_mask;int mode,pull_up_en;} gpio_config_t;
esp_err_t gpio_config(const gpio_config_t *c);
int gpio_get_level(int p);
void gpio_set_level(int p,int v);
