#include "rear_ota_recovery.h"
#include "rear_ota_datagrams.h"
#include "rear_boot_health.h"
#include "sensor_push.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFi.h>
#include <esp_http_server.h>
#include <esp_netif.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <mbedtls/md.h>
#include <atomic>
#include <lwip/sockets.h>
#include <unistd.h>
#include <settings.h>

#if __has_include("rear_ota_seed.h")
#include "rear_ota_seed.h"
#endif

// Arduino otherwise accepts a pending image during initArduino(), before setup.
extern "C" bool verifyRollbackLater() { return true; }

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
std::atomic<uint32_t> lastActivity{0};
SemaphoreHandle_t updateLock = nullptr;
std::atomic<bool> restartQueued{false};
std::atomic<const char *> bootTrial{"not_applicable"};
httpd_handle_t server = nullptr;

void rollbackTrial() {
    bootTrial = "rollback";
    const esp_err_t error = esp_ota_mark_app_invalid_rollback_and_reboot();
    log_e("Rear OTA rollback returned unexpectedly: %s", esp_err_to_name(error));
    // A reboot with a pending image still asks the bootloader to select its fallback.
    ESP.restart();
}

void watchBootTrial(void *) {
    rear_boot_health::Trial trial(millis());
    while (true) {
        uint32_t cameraAcks, sensorPackets;
        sensor_push::bootProgress(cameraAcks, sensorPackets);
        const auto action = trial.observe(millis(), WiFi.status() == WL_CONNECTED,
                                          cameraAcks, sensorPackets);
        if (action == rear_boot_health::Action::rollback) rollbackTrial();
        if (action == rear_boot_health::Action::accept &&
            esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
            bootTrial = "accepted";
            vTaskDelete(nullptr);
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void startBootTrial() {
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) != ESP_OK) return;
    if (state == ESP_OTA_IMG_VALID) bootTrial = "accepted";
    if (state != ESP_OTA_IMG_PENDING_VERIFY) return;
    if (!esp_ota_check_rollback_is_possible()) {
        // A factory/serial installation without a previous bootable slot has no fallback.
        bootTrial = esp_ota_mark_app_valid_cancel_rollback() == ESP_OK ? "unprotected" : "error";
        return;
    }
    bootTrial = "pending";
    if (xTaskCreate(watchBootTrial, "rear-boot-trial", 4096, nullptr,
                    tskIDLE_PRIORITY + 2, nullptr) != pdPASS) rollbackTrial();
}

void hexEncode(char *out, const unsigned char *bytes, size_t size) {
    constexpr char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < size; ++i) {
        out[i * 2] = digits[bytes[i] >> 4];
        out[i * 2 + 1] = digits[bytes[i] & 0xf];
    }
    out[size * 2] = '\0';
}

void formatStatus(char *json, size_t capacity) {
    const auto *running = esp_ota_get_running_partition();
    snprintf(json, capacity,
             "{\"receiver_id\":\"%s\",\"camera_id\":\"rear\",\"firmware\":\"%s\","
             "\"boot_id\":\"%s\",\"running_partition\":\"%s\",\"ota_supported\":%s,"
             "\"ota_mode\":\"%s\",\"ota_datagrams\":%s,\"ota_trial\":\"%s\"}",
             receiver, APP_VERSION, bootId, running ? running->label : "unknown",
             esp_ota_get_next_update_partition(nullptr) ? "true" : "false",
             recoveryMode ? "recovery" : "normal",
             (!recoveryMode || updateLock) ? "true" : "false", bootTrial.load());
}

esp_err_t status(httpd_req_t *req) {
    char json[352];
    formatStatus(json, sizeof(json));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, json);
}

bool validProof(const char *provided) {
    unsigned char mac[32];
    char expected[65];
    char message[96];
    snprintf(message, sizeof(message), "rear-ota-enter-v1:%s:%s", receiver, bootId);
    if (strlen(provided) != sizeof(expected) - 1 ||
        mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                        reinterpret_cast<const unsigned char *>(secret), strlen(secret),
                        reinterpret_cast<const unsigned char *>(message), strlen(message), mac) != 0)
        return false;
    hexEncode(expected, mac, sizeof(mac));
    unsigned char difference = 0;
    for (size_t i = 0; i < sizeof(expected) - 1; ++i)
        difference |= expected[i] ^ provided[i];
    return difference == 0;
}

bool queueRecoveryRestart() {
    if (recoveryMode || restartQueued.exchange(true)) return false;
    requestedMode = recoveryRequest;
    requestedModeCheck = ~recoveryRequest;
    return true;
}

void restartAfterReply() {
    delay(150);
    ESP.restart();
}

esp_err_t enter(httpd_req_t *req) {
    char provided[65]{};
    if (req->content_len != 0 ||
        httpd_req_get_hdr_value_str(req, "X-Rear-OTA-Proof", provided, sizeof(provided)) != ESP_OK ||
        !validProof(provided)) {
        httpd_resp_set_status(req, "403 Forbidden");
        return httpd_resp_sendstr(req, "Invalid maintenance proof");
    }
    if (!esp_ota_get_next_update_partition(nullptr)) {
        httpd_resp_set_status(req, "409 Conflict");
        return httpd_resp_sendstr(req, "OTA partition unavailable");
    }
    const bool restart = queueRecoveryRestart();
    httpd_resp_set_status(req, "202 Accepted");
    const esp_err_t sent = httpd_resp_sendstr(req, "OTA recovery requested");
    if (restart) restartAfterReply();
    return sent;
}

void listenForDatagrams(void *) {
    const int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        log_e("Rear OTA UDP socket could not be opened");
        vTaskDelete(nullptr);
        return;
    }
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, reinterpret_cast<const sockaddr *>(&local), sizeof(local)) != 0) {
        log_e("Rear OTA UDP socket could not be bound");
        close(fd);
        vTaskDelete(nullptr);
        return;
    }
    for (;;) {
        char packet[rear_ota_datagrams::maxRequest + 1]{};
        sockaddr_in from{};
        socklen_t length = sizeof(from);
        const int count = recvfrom(fd, packet, sizeof(packet) - 1, 0,
                                   reinterpret_cast<sockaddr *>(&from), &length);
        if (count <= 0) {
            delay(10);
            continue;
        }
        if (recoveryMode && count >= 4 && memcmp(packet, "ROT2", 4) == 0) {
            uint8_t reply[rear_ota_datagrams::maxReply];
            bool keepAwake = false;
            bool reboot = false;
            if (updateLock && xSemaphoreTake(updateLock, 0) == pdTRUE) {
                const size_t replySize = rear_ota_datagrams::handle(
                    reinterpret_cast<const uint8_t *>(packet), count, secret, bootId,
                    reply, keepAwake, reboot);
                if (keepAwake) lastActivity = millis();
                xSemaphoreGive(updateLock);
                if (replySize)
                    sendto(fd, reply, replySize, 0, reinterpret_cast<const sockaddr *>(&from), length);
                if (reboot) restartAfterReply();
            }
            continue;
        }
        constexpr char statusRequest[] = "rear-ota-status-v1";
        constexpr char enterPrefix[] = "rear-ota-enter-v1 ";
        char response[352]{};
        bool restart = false;
        if (count == sizeof(statusRequest) - 1 && strcmp(packet, statusRequest) == 0) {
            formatStatus(response, sizeof(response));
        } else if (count == sizeof(enterPrefix) - 1 + 64 &&
                   strncmp(packet, enterPrefix, sizeof(enterPrefix) - 1) == 0 &&
                   validProof(packet + sizeof(enterPrefix) - 1) &&
                   esp_ota_get_next_update_partition(nullptr)) {
            restart = queueRecoveryRestart();
            strlcpy(response, "accepted", sizeof(response));
        }
        if (response[0])
            sendto(fd, response, strlen(response), 0,
                   reinterpret_cast<const sockaddr *>(&from), length);
        if (restart) restartAfterReply();
    }
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
    startBootTrial();
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
    // The Arduino Wi-Fi driver is initialized later in a normal IotWebConf boot.
    // Initialize lwIP before opening the independent maintenance socket.
    if (recoveryMode) updateLock = xSemaphoreCreateMutex();
    if (esp_netif_init() == ESP_OK) {
        startControlServer();
        if (xTaskCreate(listenForDatagrams, "rear-ota-control", recoveryMode ? 8192 : 4096, nullptr,
                        tskIDLE_PRIORITY + 2, nullptr) != pdPASS)
            log_e("Rear OTA UDP task could not be started");
    } else log_e("Rear OTA network stack initialization failed");
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
            // Set the driver state after association, even if Arduino's
            // cached setting already says modem sleep is disabled.
            if (esp_wifi_set_ps(WIFI_PS_NONE) != ESP_OK)
                log_w("Rear OTA could not disable Wi-Fi modem sleep");
            ArduinoOTA.begin();
            listenerStarted = true;
        }
        if (!updateLock || xSemaphoreTake(updateLock, 0) == pdTRUE) {
            if (!rear_ota_datagrams::active()) ArduinoOTA.handle();
            if (updateLock) xSemaphoreGive(updateLock);
        }
    } else if (listenerStarted) {
        ArduinoOTA.end();
        listenerStarted = false;
    }
    if (millis() - lastActivity.load() >= idleWindowMs) ESP.restart();
    delay(2);
}
}
