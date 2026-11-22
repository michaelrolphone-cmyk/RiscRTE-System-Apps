#pragma once
#include <cstdint>
using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0;
constexpr esp_err_t ESP_ERR_INVALID_STATE = 0x103;
extern int testPowerReads, testPowerWrites;
extern int8_t testPower;
inline esp_err_t esp_wifi_get_max_tx_power(int8_t *power) {
    ++testPowerReads; *power = testPower; return ESP_OK;
}
inline esp_err_t esp_wifi_set_max_tx_power(int8_t power) {
    ++testPowerWrites; testPower = power; return ESP_OK;
}
