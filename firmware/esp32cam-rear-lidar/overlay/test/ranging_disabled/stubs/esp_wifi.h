#pragma once
#include <cstdint>
using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0;
constexpr esp_err_t ESP_ERR_INVALID_STATE = 0x103;
enum wifi_mode_t { WIFI_MODE_NULL = 0, WIFI_MODE_STA = 1, WIFI_MODE_AP = 2, WIFI_MODE_APSTA = 3 };
enum wifi_ps_type_t { WIFI_PS_NONE = 0, WIFI_PS_MIN_MODEM = 1, WIFI_PS_MAX_MODEM = 2 };
enum wifi_interface_t { WIFI_IF_STA, WIFI_IF_AP };
struct wifi_ap_config_t {
    uint8_t ssid[32], password[64], ssid_len, channel;
    bool ftm_responder;
    int authmode, max_connection;
};
struct wifi_config_t { wifi_ap_config_t ap; };
extern int testPowerReads, testPowerWrites;
extern int8_t testPower;
extern wifi_mode_t testMode;
extern wifi_ps_type_t testPowerSave;
extern wifi_config_t testAp;
extern int testModeReadError, testPsReadError, testApReadError;
extern int testModeSetError, testPsSetError, testApSetError;
extern int testModeWrites, testPsWrites, testApWrites;
extern bool testIgnoreDriverWrites;
inline esp_err_t esp_wifi_get_max_tx_power(int8_t *power) {
    ++testPowerReads; *power = testPower; return ESP_OK;
}
inline esp_err_t esp_wifi_set_max_tx_power(int8_t power) {
    ++testPowerWrites; testPower = power; return ESP_OK;
}
inline esp_err_t esp_wifi_get_mode(wifi_mode_t *mode) {
    if (!testModeReadError) *mode = testMode;
    return testModeReadError;
}
inline esp_err_t esp_wifi_get_ps(wifi_ps_type_t *power) {
    if (!testPsReadError) *power = testPowerSave;
    return testPsReadError;
}
inline esp_err_t esp_wifi_get_config(wifi_interface_t, wifi_config_t *config) {
    if (!testApReadError) *config = testAp;
    return testApReadError;
}
inline esp_err_t esp_wifi_set_mode(wifi_mode_t mode) {
    ++testModeWrites;
    if (!testModeSetError && !testIgnoreDriverWrites) testMode = mode;
    return testModeSetError;
}
inline esp_err_t esp_wifi_set_ps(wifi_ps_type_t power) {
    ++testPsWrites;
    if (!testPsSetError && !testIgnoreDriverWrites) testPowerSave = power;
    return testPsSetError;
}
inline esp_err_t esp_wifi_set_config(wifi_interface_t, wifi_config_t *config) {
    ++testApWrites;
    if (!testApSetError && !testIgnoreDriverWrites) testAp = *config;
    return testApSetError;
}
