#include "rear_ranging.h"
#include "settings.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include <string>
#include <sys/time.h>

namespace rear_ranging {
namespace {
constexpr char DISABLED_REASON[] = "disabled_in_firmware";
constexpr int8_t TX_POWER_QUARTER_DBM = 60;
WebServer *httpServer = nullptr;
bool (*authorizeRequest)() = nullptr;
char receiverId[25] = {};
char bootId[17] = {};
std::atomic<bool> paused{false};
std::atomic<int> actualTxPower{0}, txPowerReadError{ESP_ERR_INVALID_STATE};

// The rear keeps its established power limit independently of ranging. This
// worker never changes Wi-Fi mode, starts an AP, scans, or initiates FTM.
void updateTxPower() {
    if (paused.load() || WiFi.status() != WL_CONNECTED) return;
    int8_t power = 0;
    if (esp_wifi_get_max_tx_power(&power) != ESP_OK || power != TX_POWER_QUARTER_DBM)
        esp_wifi_set_max_tx_power(TX_POWER_QUARTER_DBM);
    const esp_err_t result = esp_wifi_get_max_tx_power(&power);
    actualTxPower.store(power);
    txPowerReadError.store(result);
}

void powerWorker(void *) {
    for (;;) {
        updateTxPower();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void putCapabilities(JsonObject output) {
    output["ftm"] = false;
    output["ftm_initiator"] = false;
    output["ftm_responder"] = false;
    output["ahrs"] = false;
    output["ultrasonic"] = false;
}

void putDisabledRanging(JsonObject output) {
    output["ranging_available"] = false;
    output["ranging_enabled"] = false;
    output["ranging_disabled_reason"] = DISABLED_REASON;
    putCapabilities(output.createNestedObject("capabilities"));
    JsonObject ftm = output.createNestedObject("ftm");
    ftm["supported"] = false;
    ftm["enabled"] = false;
    ftm["status"] = DISABLED_REASON;
    ftm["report_id"] = 0;
    ftm["busy"] = false;
    ftm["retiring"] = false;
    ftm["cancel_error"] = 0;
    ftm["recovery"] = nullptr;
    ftm["target"] = nullptr;
    ftm["channel"] = nullptr;
    ftm["distance_method"] = "wifi_ftm";
    ftm["age_ms"] = nullptr;
    ftm["rtt_ns"] = nullptr;
    ftm["distance_m"] = nullptr;
    ftm["samples"] = 0;
    JsonObject responder = output.createNestedObject("ftm_responder");
    responder["supported"] = false;
    responder["enabled"] = false;
    responder["active"] = false;
    responder["status"] = DISABLED_REASON;
    responder["bssid"] = nullptr;
    responder["ssid"] = nullptr;
    responder["channel"] = nullptr;
    responder["station_connected"] = WiFi.status() == WL_CONNECTED;
    responder["error"] = 0;
    responder["role"] = "rover";
    responder["mode"] = "disabled";
    responder["firmware"] = APP_VERSION;
    output["tx_power_requested_dbm"] = TX_POWER_QUARTER_DBM / 4.0;
    const int error = txPowerReadError.load();
    output["tx_power_read_error"] = error;
    if (error == ESP_OK) output["tx_power_actual_dbm"] = actualTxPower.load() / 4.0;
    else output["tx_power_actual_dbm"] = nullptr;
}

bool authorized() {
    if (!httpServer) return false;
    if (authorizeRequest) return authorizeRequest();
    httpServer->send(403, "application/json", "{\"error\":\"Authentication callback unavailable\"}");
    return false;
}

void sendJson(const JsonDocument &document, int code = 200) {
    std::string text;
    serializeJson(document, text);
    httpServer->sendHeader("Cache-Control", "no-store");
    httpServer->send(code, "application/json", text.c_str());
}

void handleSnapshot() {
    if (!authorized()) return;
    DynamicJsonDocument document(2048);
    JsonObject output = document.to<JsonObject>();
    output["schema"] = 1;
    output["receiver_id"] = receiverId;
    output["boot_id"] = bootId;
    output["uptime_ms"] = millis();
    output["sequence"] = 0;
    output["paused"] = paused.load();
    timeval clock;
    gettimeofday(&clock, nullptr);
    if (clock.tv_sec >= 1700000000)
        output["timestamp_ms"] = static_cast<uint64_t>(clock.tv_sec) * 1000ULL + clock.tv_usec / 1000;
    else output["timestamp_ms"] = nullptr;
    putDisabledRanging(output);
    JsonObject config = output.createNestedObject("config");
    config["ftm_initiator_enabled"] = false;
    config["ftm_responder_enabled"] = false;
    config["mutable"] = false;
    config["disabled_reason"] = DISABLED_REASON;
    JsonObject status = output.createNestedObject("status");
    status["worker_ready"] = false;
    sendJson(document);
}

void handleDisabledWrite() {
    if (!authorized()) return;
    DynamicJsonDocument document(256);
    document["error"] = "Wi-Fi ranging is disabled in this rear firmware";
    document["reason"] = DISABLED_REASON;
    document["firmware"] = APP_VERSION;
    sendJson(document, 501);
}
} // namespace

void writeJson(JsonObject output, uint64_t captureUs) {
    (void)captureUs;
    // Never expose a saved or previous-boot range as a current observation.
    putDisabledRanging(output);
}

void begin(WebServer &server, bool (*authorize)()) {
    if (httpServer) return;
    httpServer = &server;
    authorizeRequest = authorize;
    snprintf(receiverId, sizeof(receiverId), "rear-%012llx",
             static_cast<unsigned long long>(ESP.getEfuseMac()));
    snprintf(bootId, sizeof(bootId), "%08lx%08lx",
             static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));
    server.on("/api/ranging", HTTP_GET, handleSnapshot);
    server.on("/api/telemetry/config", HTTP_GET, handleSnapshot);
    server.on("/api/telemetry/config", HTTP_POST, handleDisabledWrite);
    server.on("/api/telemetry/ftm", HTTP_POST, handleDisabledWrite);
    // Saved rear-ranging Preferences are deliberately not loaded. IotWebConf
    // remains the sole owner of the provisioning/recovery AP; no ranging SSID
    // is advertised and no FTM event handler or ranging worker is installed.
    xTaskCreate(powerWorker, "rear-txpower", 2048, nullptr, 1, nullptr);
}

void pause(bool value) {
    paused.store(value);
}
} // namespace rear_ranging
