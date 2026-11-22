#include "rear_ranging.h"
#include "settings.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <atomic>
#include <string>
#include <cstring>
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
SemaphoreHandle_t networkOwner = nullptr;
char legacyRangingSsid[33] = {};
portMUX_TYPE snapshotLock = portMUX_INITIALIZER_UNLOCKED;
struct RadioState {
    bool observed = false, stationConnected = false, apFtm = false, legacyAp = false;
    uint32_t sampledAtMs = 0, sequence = 0, deferred = 0;
    wifi_mode_t mode = WIFI_MODE_NULL;
    wifi_ps_type_t powerSave = WIFI_PS_NONE;
    int modeError = ESP_ERR_INVALID_STATE, powerSaveError = ESP_ERR_INVALID_STATE;
    int apConfigError = ESP_ERR_INVALID_STATE, txPowerError = ESP_ERR_INVALID_STATE;
    int8_t txPower = 0;
    uint32_t sleepAttempts = 0, sleepCorrections = 0;
    uint32_t apFtmAttempts = 0, apFtmCorrections = 0;
    uint32_t legacyApAttempts = 0, legacyApRetirements = 0;
    int lastSleepError = ESP_OK, lastApFtmError = ESP_OK, lastLegacyApError = ESP_OK;
    wifi_ps_type_t lastSleepObserved = WIFI_PS_NONE;
};
RadioState radioState;

RadioState readSnapshot() {
    portENTER_CRITICAL(&snapshotLock);
    const RadioState result = radioState;
    portEXIT_CRITICAL(&snapshotLock);
    return result;
}
void publishSnapshot(const RadioState &state) {
    portENTER_CRITICAL(&snapshotLock);
    radioState = state;
    portEXIT_CRITICAL(&snapshotLock);
}

bool isLegacyAp(const wifi_config_t &config) {
    const size_t expected = strlen(legacyRangingSsid);
    const size_t length = config.ap.ssid_len ? config.ap.ssid_len :
        strnlen(reinterpret_cast<const char *>(config.ap.ssid), sizeof(config.ap.ssid));
    return expected && length == expected &&
        memcmp(config.ap.ssid, legacyRangingSsid, expected) == 0;
}

void readDriver(RadioState &state, wifi_config_t &ap) {
    state.stationConnected = WiFi.status() == WL_CONNECTED;
    state.modeError = esp_wifi_get_mode(&state.mode);
    state.powerSaveError = esp_wifi_get_ps(&state.powerSave);
    memset(&ap, 0, sizeof(ap));
    state.apConfigError = esp_wifi_get_config(WIFI_IF_AP, &ap);
    state.apFtm = state.apConfigError == ESP_OK && ap.ap.ftm_responder;
    state.legacyAp = state.apConfigError == ESP_OK && isLegacyAp(ap);
    state.txPowerError = esp_wifi_get_max_tx_power(&state.txPower);
}

struct NetworkGuard {
    bool held;
    NetworkGuard() : held(networkOwner &&
        xSemaphoreTake(networkOwner, pdMS_TO_TICKS(20)) == pdTRUE) {}
    ~NetworkGuard() { if (held) xSemaphoreGive(networkOwner); }
};

// IotWebConf and corrective writes share ownership. The driver snapshot uses a
// separate short critical section so an HTTP status callback cannot self-deadlock.
void updateRadioState() {
    if (paused.load()) return;
    RadioState next = readSnapshot();
    NetworkGuard guard;
    if (!guard.held) {
        ++next.deferred;
        publishSnapshot(next);
        return;
    }
    if (paused.load()) return;
    wifi_config_t ap = {};
    readDriver(next, ap);
    bool sleepAttempted = false, apAttempted = false, retireAttempted = false;
    const bool station = next.stationConnected && next.modeError == ESP_OK &&
        (next.mode & WIFI_MODE_STA);
    if (station) {
        if (next.powerSaveError == ESP_OK && next.powerSave != WIFI_PS_NONE) {
            sleepAttempted = true;
            ++next.sleepAttempts;
            next.lastSleepObserved = next.powerSave;
            // The Arduino setSleep wrapper can skip this call when its cached
            // preference already says NONE. Enforce against actual driver state.
            next.lastSleepError = esp_wifi_set_ps(WIFI_PS_NONE);
        }
        if ((next.mode & WIFI_MODE_AP) && next.apConfigError == ESP_OK) {
            if (ap.ap.ftm_responder) {
                apAttempted = true;
                ++next.apFtmAttempts;
                ap.ap.ftm_responder = false;
                next.lastApFtmError = esp_wifi_set_config(WIFI_IF_AP, &ap);
            }
            // Exact identity of this board's old ranging AP. Ordinary recovery
            // and provisioning APs keep their mode, SSID, password and config.
            if (next.mode == WIFI_MODE_APSTA && isLegacyAp(ap)) {
                retireAttempted = true;
                ++next.legacyApAttempts;
                next.lastLegacyApError = esp_wifi_set_mode(WIFI_MODE_STA);
            }
        }
        if (next.txPowerError != ESP_OK || next.txPower != TX_POWER_QUARTER_DBM)
            esp_wifi_set_max_tx_power(TX_POWER_QUARTER_DBM);
        // Read back every corrective write before reporting state or success.
        readDriver(next, ap);
    }
    if (sleepAttempted && next.lastSleepError == ESP_OK &&
        next.powerSaveError == ESP_OK && next.powerSave == WIFI_PS_NONE)
        ++next.sleepCorrections;
    if (apAttempted && next.lastApFtmError == ESP_OK &&
        next.apConfigError == ESP_OK && !next.apFtm)
        ++next.apFtmCorrections;
    if (retireAttempted && next.lastLegacyApError == ESP_OK &&
        next.modeError == ESP_OK && next.mode == WIFI_MODE_STA)
        ++next.legacyApRetirements;
    next.observed = true;
    next.sampledAtMs = millis();
    ++next.sequence;
    publishSnapshot(next);
}

void powerWorker(void *) {
    for (;;) {
        updateRadioState();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void putDriverState(JsonObject output, const RadioState &state) {
    output["observed"] = state.observed;
    output["ownership_guard_available"] = networkOwner != nullptr;
    output["maintenance_paused"] = paused.load();
    output["sample_sequence"] = state.sequence;
    if (state.observed) {
        output["sampled_monotonic_ms"] = state.sampledAtMs;
        output["sample_age_ms"] = static_cast<uint32_t>(millis() - state.sampledAtMs);
        output["station_connected"] = state.stationConnected;
    } else {
        output["sampled_monotonic_ms"] = nullptr;
        output["sample_age_ms"] = nullptr;
        output["station_connected"] = nullptr;
    }
    if (state.modeError == ESP_OK) {
        output["mode"] = static_cast<int>(state.mode);
        output["ap_active"] = static_cast<bool>(state.mode & WIFI_MODE_AP);
    } else {
        output["mode"] = nullptr;
        output["ap_active"] = nullptr;
    }
    output["mode_read_error"] = state.modeError;
    output["power_save_requested"] = static_cast<int>(WIFI_PS_NONE);
    if (state.powerSaveError == ESP_OK) output["power_save"] = static_cast<int>(state.powerSave);
    else output["power_save"] = nullptr;
    output["power_save_read_error"] = state.powerSaveError;
    if (state.apConfigError == ESP_OK) {
        output["ap_ftm_responder"] = state.apFtm;
        output["legacy_ranging_ap_configured"] = state.legacyAp;
    } else {
        output["ap_ftm_responder"] = nullptr;
        output["legacy_ranging_ap_configured"] = nullptr;
    }
    if (state.apConfigError == ESP_OK && state.modeError == ESP_OK)
        output["legacy_ranging_ap_active"] = state.legacyAp && (state.mode & WIFI_MODE_AP);
    else output["legacy_ranging_ap_active"] = nullptr;
    output["ap_config_read_error"] = state.apConfigError;
    output["ownership_deferrals"] = state.deferred;
    output["power_save_set_attempts"] = state.sleepAttempts;
    output["power_save_corrections"] = state.sleepCorrections;
    output["ap_ftm_clear_attempts"] = state.apFtmAttempts;
    output["ap_ftm_corrections"] = state.apFtmCorrections;
    output["legacy_ap_retire_attempts"] = state.legacyApAttempts;
    output["legacy_ap_retirements"] = state.legacyApRetirements;
    if (state.sleepAttempts) {
        output["last_power_save_set_error"] = state.lastSleepError;
        output["last_power_save_observed"] = static_cast<int>(state.lastSleepObserved);
    } else {
        output["last_power_save_set_error"] = nullptr;
        output["last_power_save_observed"] = nullptr;
    }
    if (state.apFtmAttempts) output["last_ap_ftm_clear_error"] = state.lastApFtmError;
    else output["last_ap_ftm_clear_error"] = nullptr;
    if (state.legacyApAttempts) output["last_legacy_ap_retire_error"] = state.lastLegacyApError;
    else output["last_legacy_ap_retire_error"] = nullptr;
    // No SSID, password or other AP configuration content leaves this function.
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
    const RadioState state = readSnapshot();
    output["tx_power_read_error"] = state.txPowerError;
    if (state.txPowerError == ESP_OK) output["tx_power_actual_dbm"] = state.txPower / 4.0;
    else output["tx_power_actual_dbm"] = nullptr;
    putDriverState(output.createNestedObject("wifi_driver"), state);
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
    DynamicJsonDocument document(4096);
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
    networkOwner = xSemaphoreCreateMutex();
    const uint32_t suffix = static_cast<uint32_t>(ESP.getEfuseMac());
    snprintf(legacyRangingSsid, sizeof(legacyRangingSsid), "C3-TOF-ROVER-REAR-%08lx",
             static_cast<unsigned long>(suffix));
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
    xTaskCreate(powerWorker, "rear-txpower", 4096, nullptr, 1, nullptr);
}

bool beginNetworkLoop() {
    // Camera/OTA work continues even while maintenance holds the network owner.
    return !networkOwner || xSemaphoreTake(networkOwner, 0) == pdTRUE;
}

void endNetworkLoop() {
    if (networkOwner) xSemaphoreGive(networkOwner);
}

void pause(bool value) {
    paused.store(value);
}
} // namespace rear_ranging
