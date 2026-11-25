#include "rear_ota_recovery.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFi.h>
#include <esp_http_server.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <mbedtls/md.h>
#include <settings.h>

#if __has_include("rear_ota_seed.h")
#include "rear_ota_seed.h"
#endif

namespace rear_ota_recovery {
namespace {
constexpr uint32_t recoveryRequest = 0x524f5441;
constexpr uint32_t idleWindowMs = 5 * 60 * 1000;
constexpr uint16_t port = 3233;
constexpr char preferencesNamespace[] = "rear-ota";
RTC_NOINIT_ATTR uint32_t requestedMode;
RTC_NOINIT_ATTR uint32_t requestedModeCheck;
char secret[65]{};
char bootId[33]{};
char receiver[18]{};
bool recoveryMode = false;
bool listenerStarted = false;
uint32_t lastActivity = 0;
httpd_handle_t server = nullptr;

void hexEncode(char *out, const unsigned char *bytes, size_t size) {
    constexpr char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < size; ++i) {
        out[i * 2] = digits[bytes[i] >> 4];
        out[i * 2 + 1] = digits[bytes[i] & 0xf];
    }
    out[size * 2] = '\0';
}

esp_err_t status(httpd_req_t *req) {
    const auto *running = esp_ota_get_running_partition();
    char json[352];
    snprintf(json, sizeof(json),
             "{\"receiver_id\":\"%s\",\"camera_id\":\"rear\",\"firmware\":\"%s\","
             "\"boot_id\":\"%s\",\"running_partition\":\"%s\",\"ota_supported\":%s,"
             "\"ota_mode\":\"%s\"}",
             receiver, APP_VERSION, bootId, running ? running->label : "unknown",
             esp_ota_get_next_update_partition(nullptr) ? "true" : "false",
             recoveryMode ? "recovery" : "normal");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, json);
}

esp_err_t enter(httpd_req_t *req) {
    char provided[65]{};
    unsigned char mac[32];
    char expected[65];
    char message[96];
    snprintf(message, sizeof(message), "rear-ota-enter-v1:%s:%s", receiver, bootId);
    if (req->content_len != 0 ||
        httpd_req_get_hdr_value_str(req, "X-Rear-OTA-Proof", provided, sizeof(provided)) != ESP_OK ||
        strlen(provided) != sizeof(provided) - 1 ||
        mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                        reinterpret_cast<const unsigned char *>(secret), strlen(secret),
                        reinterpret_cast<const unsigned char *>(message), strlen(message), mac) != 0) {
        httpd_resp_set_status(req, "403 Forbidden");
        return httpd_resp_sendstr(req, "Invalid maintenance proof");
    }
    hexEncode(expected, mac, sizeof(mac));
    unsigned char difference = 0;
    for (size_t i = 0; i < sizeof(expected) - 1; ++i)
        difference |= expected[i] ^ provided[i];
    if (difference) {
        httpd_resp_set_status(req, "403 Forbidden");
        return httpd_resp_sendstr(req, "Invalid maintenance proof");
    }
    if (recoveryMode) return httpd_resp_sendstr(req, "OTA recovery already active");
    if (!esp_ota_get_next_update_partition(nullptr)) {
        httpd_resp_set_status(req, "409 Conflict");
        return httpd_resp_sendstr(req, "OTA partition unavailable");
    }
    requestedMode = recoveryRequest;
    requestedModeCheck = ~recoveryRequest;
    httpd_resp_set_status(req, "202 Accepted");
    httpd_resp_sendstr(req, "Restarting into OTA recovery");
    delay(150);
    ESP.restart();
    return ESP_OK;
}

void startControlServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;
    config.max_open_sockets = 2;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 2;
    config.send_wait_timeout = 2;
    if (httpd_start(&server, &config) != ESP_OK) {
        log_e("Rear OTA control server failed to start");
        return;
    }
    httpd_uri_t getStatus{};
    getStatus.uri = "/api/rear";
    getStatus.method = HTTP_GET;
    getStatus.handler = status;
    httpd_uri_t enterMode{};
    enterMode.uri = "/api/ota/recovery";
    enterMode.method = HTTP_POST;
    enterMode.handler = enter;
    if (httpd_register_uri_handler(server, &getStatus) != ESP_OK ||
        httpd_register_uri_handler(server, &enterMode) != ESP_OK) {
        httpd_stop(server);
        server = nullptr;
        log_e("Rear OTA control routes failed to register");
    }
}

void loadKey() {
    Preferences preferences;
#ifdef REAR_OTA_SEED
    if (preferences.begin(preferencesNamespace, false)) {
        // The USB image can enroll a new unit. A subsequent boot keeps the enrolled key.
        if (!preferences.isKey("key"))
            preferences.putString("key", REAR_OTA_SEED);
        preferences.end();
    }
#endif
    if (preferences.begin(preferencesNamespace, true)) {
        preferences.getString("key", secret, sizeof(secret));
        preferences.end();
    }
    if (strlen(secret) != sizeof(secret) - 1) secret[0] = '\0';
}
}

bool begin(const char *hostname, const char *ssid, const char *wifiPassword) {
    const bool requested = esp_reset_reason() == ESP_RST_SW &&
        requestedMode == recoveryRequest && requestedModeCheck == ~recoveryRequest;
    requestedMode = 0;
    requestedModeCheck = 0;
    loadKey();
    if (!secret[0]) return false;

    unsigned char random[16];
    esp_fill_random(random, sizeof(random));
    hexEncode(bootId, random, sizeof(random));
    snprintf(receiver, sizeof(receiver), "rear-%012llx",
             static_cast<unsigned long long>(ESP.getEfuseMac()));
    recoveryMode = requested && ssid && ssid[0];
    startControlServer();
    if (!recoveryMode) return false;

    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(hostname);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);
    WiFi.begin(ssid, wifiPassword);
    ArduinoOTA.setHostname(hostname);
    ArduinoOTA.setMdnsEnabled(false);
    ArduinoOTA.setPassword(secret);
    ArduinoOTA.setTimeout(30000);
    ArduinoOTA.onStart([]() { lastActivity = millis(); });
    ArduinoOTA.onProgress([](unsigned int, unsigned int) { lastActivity = millis(); });
    ArduinoOTA.onError([](ota_error_t error) {
        if (Update.isRunning()) Update.abort();
        if (error != OTA_AUTH_ERROR) lastActivity = millis();
        log_e("Rear OTA recovery transfer error: %u", static_cast<unsigned>(error));
    });
    lastActivity = millis();
    return true;
}

bool enrolled() { return secret[0] != '\0'; }

void service() {
    if (WiFi.status() == WL_CONNECTED) {
        if (!listenerStarted) {
            ArduinoOTA.begin();
            listenerStarted = true;
        }
        ArduinoOTA.handle();
    } else if (listenerStarted) {
        ArduinoOTA.end();
        listenerStarted = false;
    }
    if (millis() - lastActivity >= idleWindowMs) ESP.restart();
    delay(2);
}
}
