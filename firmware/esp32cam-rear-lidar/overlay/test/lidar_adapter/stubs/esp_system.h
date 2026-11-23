#pragma once
#include <cstdint>
// Same public enum values as the pinned ESP-IDF v4.4.7 esp_system.h.
enum esp_reset_reason_t {
 ESP_RST_UNKNOWN=0,ESP_RST_POWERON,ESP_RST_EXT,ESP_RST_SW,ESP_RST_PANIC,
 ESP_RST_INT_WDT,ESP_RST_TASK_WDT,ESP_RST_WDT,ESP_RST_DEEPSLEEP,
 ESP_RST_BROWNOUT,ESP_RST_SDIO
};
esp_reset_reason_t esp_reset_reason();
uint32_t esp_random();
