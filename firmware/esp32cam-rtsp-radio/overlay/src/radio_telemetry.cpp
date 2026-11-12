#include "radio_telemetry.h"
#include "radio_telemetry_model.h"
#include "ahrs_telemetry.h"
#include "ultrasonic_telemetry.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <sys/time.h>
#include <cmath>
#include <cstring>
#include <cstdlib>

#if defined(CONFIG_BT_ENABLED) && CONFIG_BT_ENABLED && !defined(CONFIG_IDF_TARGET_ESP32S2)
#define RADIO_HAS_BLE 1
#include <NimBLEDevice.h>
#else
#define RADIO_HAS_BLE 0
#endif

#if defined(CONFIG_ESP_WIFI_FTM_INITIATOR_SUPPORT) && CONFIG_ESP_WIFI_FTM_INITIATOR_SUPPORT
#define RADIO_HAS_FTM 1
#else
#define RADIO_HAS_FTM 0
#endif

namespace radio_telemetry {
namespace {
using radio_telemetry_model::IBeacon;
constexpr size_t WIFI_CAPACITY = 32;
constexpr size_t BLE_CAPACITY = 64;
constexpr uint32_t OBSERVATION_TTL_MS = 300000;
constexpr uint32_t FTM_TIMEOUT_MS = 12000;
constexpr uint32_t WIFI_SCAN_TIMEOUT_MS = 10000;
constexpr float RSSI_ALPHA = 0.25f;

struct Config {
    bool wifiEnabled = true;
    bool bleEnabled = RADIO_HAS_BLE;
    uint32_t wifiInterval = 60000;
    uint32_t bleInterval = 10000;
    uint32_t bleDuration = 2000;
    uint32_t publishInterval = 5000;
    uint32_t ahrsPublishInterval = 100;
    char udpHost[16] = {};
    uint16_t udpPort = 9876;
    float wifiReference = -45.0f;
    float bleReference = -59.0f;
    float exponent = 2.5f;
    bool wifiCalibrated = false;
    bool bleCalibrated = false;
};

struct Observation {
    bool used = false;
    uint8_t address[6] = {};
    uint8_t addressType = 0;
    char name[33] = {};
    uint8_t channel = 0;
    int8_t rssi = 0;
    float filteredRssi = 0;
    uint32_t samples = 0;
    uint32_t lastSeen = 0;
    bool ftmResponder = false;
    bool haveTxPower = false;
    int8_t txPower = 0;
    bool haveIBeacon = false;
    IBeacon iBeacon = {};
};

struct Status {
    bool workerReady = false;
    bool wifiScan = false;
    uint8_t wifiScanChannel = 0;
    bool bleScan = false;
    bool bleInitialized = false;
    bool bleInitializing = false;
    bool wifiScanned = false;
    bool bleScanned = false;
    uint32_t wifiLastScan = 0;
    uint32_t bleLastScan = 0;
    int wifiError = 0;
    int bleError = 0;
    int bleEndReason = 0;
    uint32_t udpSent = 0;
    uint32_t udpFailed = 0;
    uint32_t wifiEvictions = 0;
    uint32_t bleEvictions = 0;
    uint32_t wifiTruncated = 0;
};

struct FtmState {
    bool busy = false;
    bool pending = false;
    bool hasReport = false;
    bool successful = false;
    bool retiring = false;
    uint32_t retiredAt = 0;
    uint32_t generation = 0;
    int cancelError = 0;
    char stopReason[24] = {};
    uint8_t target[6] = {};
    uint8_t channel = 0;
    char status[32] = "never_requested";
    uint32_t started = 0;
    uint32_t completed = 0;
    uint32_t rtt = 0;
    uint32_t distanceCm = 0;
    uint8_t samples = 0;
};

Config configuration;
Observation wifiObservations[WIFI_CAPACITY];
Observation bleObservations[BLE_CAPACITY];
Status status;
FtmState ftm;
bool isPaused = false;
uint32_t configurationRevision = 0;
uint32_t packetSequence = 0;
uint32_t ftmGeneration = 0;
SemaphoreHandle_t stateMutex = nullptr;
TaskHandle_t workerHandle = nullptr;
WebServer *httpServer = nullptr;
bool (*authorizeRequest)() = nullptr;
char receiverId[25] = {};
char bootId[17] = {};
#if RADIO_HAS_BLE
NimBLEScan *readyBleScan = nullptr; // published only under stateMutex
#endif

struct Lock {
    Lock() { xSemaphoreTake(stateMutex, portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(stateMutex); }
};

bool pauseRequested() {
    Lock lock;
    return isPaused;
}

void formatMac(const uint8_t address[6], char output[18]) {
    snprintf(output, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
             address[0], address[1], address[2], address[3], address[4], address[5]);
}

// Keep valid complete UTF-8 sequences. Advertising data is not trusted text.
void copyName(char output[33], const uint8_t *input, size_t size) {
    size_t in = 0, out = 0;
    while (in < size && out < 32) {
        uint8_t value = input[in];
        if (value < 0x80) {
            output[out++] = value >= 0x20 && value != 0x7f ? value : ' ';
            ++in;
            continue;
        }
        size_t width = value >= 0xc2 && value <= 0xdf ? 2 :
                       value >= 0xe0 && value <= 0xef ? 3 :
                       value >= 0xf0 && value <= 0xf4 ? 4 : 0;
        bool valid = width && in + width <= size;
        for (size_t j = 1; valid && j < width; ++j)
            valid = (input[in + j] & 0xc0) == 0x80;
        if (valid && width == 3)
            valid = !((value == 0xe0 && input[in + 1] < 0xa0) ||
                      (value == 0xed && input[in + 1] >= 0xa0));
        if (valid && width == 4)
            valid = !((value == 0xf0 && input[in + 1] < 0x90) ||
                      (value == 0xf4 && input[in + 1] >= 0x90));
        if (!valid) { output[out++] = '?'; ++in; continue; }
        if (out + width > 32) break;
        memcpy(output + out, input + in, width);
        out += width;
        in += width;
    }
    output[out] = 0;
}

void putConfig(JsonObject output, const Config &config) {
    output["wifi_enabled"] = config.wifiEnabled;
    output["ble_enabled"] = config.bleEnabled;
    output["wifi_interval_ms"] = config.wifiInterval;
    output["ble_interval_ms"] = config.bleInterval;
    output["ble_duration_ms"] = config.bleDuration;
    output["publish_interval_ms"] = config.publishInterval;
    output["ahrs_publish_interval_ms"] = config.ahrsPublishInterval;
    output["udp_host"] = String(config.udpHost);
    output["udp_port"] = config.udpPort;
    output["wifi_reference_rssi_1m_dbm"] = config.wifiReference;
    output["ble_reference_rssi_1m_dbm"] = config.bleReference;
    output["path_loss_exponent"] = config.exponent;
    output["wifi_calibrated"] = config.wifiCalibrated;
    output["ble_calibrated"] = config.bleCalibrated;
}

bool readBool(JsonVariantConst value, bool &target) {
    if (!value.is<bool>()) return false;
    target = value.as<bool>();
    return true;
}

bool readUnsigned(JsonVariantConst value, uint32_t &target, uint32_t low, uint32_t high) {
    if (!value.is<uint32_t>()) return false;
    uint32_t result = value.as<uint32_t>();
    if (result < low || result > high) return false;
    target = result;
    return true;
}

bool readFloat(JsonVariantConst value, float &target, float low, float high) {
    if (!value.is<float>() && !value.is<int>()) return false;
    float result = value.as<float>();
    if (!std::isfinite(result) || result < low || result > high) return false;
    target = result;
    return true;
}

bool validDestination(const char *text) {
    if (!text || !text[0]) return true; // empty disables publication
    IPAddress destination;
    if (!destination.fromString(text)) return false;
    // Numeric unicast IPv4 keeps publication bounded and avoids DNS work.
    return destination[0] != 0 && destination[0] != 127 && destination[0] < 224 &&
           destination != IPAddress(255, 255, 255, 255);
}

bool patchConfig(JsonObjectConst input, Config &config, String &error) {
    for (JsonPairConst item : input) {
        const char *key = item.key().c_str();
        JsonVariantConst value = item.value();
        bool valid = false;
        if (!strcmp(key, "wifi_enabled")) valid = readBool(value, config.wifiEnabled);
        else if (!strcmp(key, "ble_enabled")) valid = readBool(value, config.bleEnabled);
        else if (!strcmp(key, "wifi_interval_ms")) valid = readUnsigned(value, config.wifiInterval, 10000, 86400000);
        else if (!strcmp(key, "ble_interval_ms")) valid = readUnsigned(value, config.bleInterval, 1000, 86400000);
        else if (!strcmp(key, "ble_duration_ms")) valid = readUnsigned(value, config.bleDuration, 200, 5000);
        else if (!strcmp(key, "publish_interval_ms")) valid = readUnsigned(value, config.publishInterval, 1000, 86400000);
        else if (!strcmp(key, "ahrs_publish_interval_ms")) valid = readUnsigned(value, config.ahrsPublishInterval, 50, 86400000);
        else if (!strcmp(key, "wifi_reference_rssi_1m_dbm")) valid = readFloat(value, config.wifiReference, -127, -1);
        else if (!strcmp(key, "ble_reference_rssi_1m_dbm")) valid = readFloat(value, config.bleReference, -127, -1);
        else if (!strcmp(key, "path_loss_exponent")) valid = readFloat(value, config.exponent, 0.5f, 6.0f);
        else if (!strcmp(key, "wifi_calibrated")) valid = readBool(value, config.wifiCalibrated);
        else if (!strcmp(key, "ble_calibrated")) valid = readBool(value, config.bleCalibrated);
        else if (!strcmp(key, "udp_port")) {
            uint32_t port = config.udpPort;
            valid = readUnsigned(value, port, 1, 65535);
            if (valid) config.udpPort = static_cast<uint16_t>(port);
        } else if (!strcmp(key, "udp_host")) {
            const char *host = value.is<const char *>() ? value.as<const char *>() : nullptr;
            valid = host && strlen(host) < sizeof(config.udpHost) && validDestination(host);
            if (valid) snprintf(config.udpHost, sizeof(config.udpHost), "%s", host);
        } else {
            error = String("Unknown configuration key: ") + key;
            return false;
        }
        if (!valid) {
            error = String("Invalid configuration value: ") + key;
            return false;
        }
    }
    if (config.bleDuration >= config.bleInterval) {
        error = "ble_duration_ms must be less than ble_interval_ms";
        return false;
    }
    return true;
}

void loadConfig() {
    Preferences preferences;
    if (!preferences.begin("radio-telemetry", true)) return;
    String saved = preferences.getString("settings", "");
    preferences.end();
    if (!saved.length() || saved.length() > 2048) return;
    DynamicJsonDocument document(2048);
    if (deserializeJson(document, saved) || !document.is<JsonObject>()) return;
    Config candidate;
    String error;
    if (patchConfig(document.as<JsonObjectConst>(), candidate, error))
        configuration = candidate;
}

bool saveConfig(const Config &config) {
    DynamicJsonDocument document(2048);
    putConfig(document.to<JsonObject>(), config);
    if (document.overflowed()) return false;
    String serialized;
    serializeJson(document, serialized);
    if (serialized.length() != measureJson(document)) return false;
    Preferences preferences;
    if (!preferences.begin("radio-telemetry", false)) return false;
    size_t count = preferences.putString("settings", serialized);
    preferences.end();
    return count == serialized.length();
}

Observation &slotFor(Observation *items, size_t capacity, const uint8_t address[6],
                     uint8_t addressType, uint32_t now, uint32_t &evictions) {
    size_t oldest = 0;
    uint32_t oldestAge = 0;
    size_t unused = capacity;
    for (size_t i = 0; i < capacity; ++i) {
        if (items[i].used && items[i].addressType == addressType &&
            !memcmp(items[i].address, address, 6))
            return items[i];
        if (!items[i].used && unused == capacity) unused = i;
        if (items[i].used && now - items[i].lastSeen >= oldestAge) {
            oldestAge = now - items[i].lastSeen;
            oldest = i;
        }
    }
    size_t selected = unused != capacity ? unused : oldest;
    if (items[selected].used) ++evictions;
    items[selected] = Observation();
    items[selected].used = true;
    memcpy(items[selected].address, address, 6);
    items[selected].addressType = addressType;
    return items[selected];
}

void observeRssi(Observation &observation, int8_t rssi, uint32_t now) {
    observation.rssi = rssi;
    observation.filteredRssi = observation.samples ?
        observation.filteredRssi + RSSI_ALPHA * (rssi - observation.filteredRssi) : rssi;
    if (observation.samples != UINT32_MAX) ++observation.samples;
    observation.lastSeen = now;
}

bool copyObservation(bool bluetooth, size_t index, Observation &copy) {
    Lock lock;
    Observation &source = bluetooth ? bleObservations[index] : wifiObservations[index];
    if (!source.used) return false;
    copy = source;
    return true;
}

void putObservation(JsonObject output, const Observation &observation, bool bluetooth, const Config &config) {
    char address[18], id[24];
    formatMac(observation.address, address);
    if (bluetooth) snprintf(id, sizeof(id), "%s/%u", address, observation.addressType);
    else snprintf(id, sizeof(id), "%s", address);
    // ArduinoJson duplicates these stack-backed strings.
    output["id"] = String(id);
    output["address"] = String(address);
    output["name"] = String(observation.name);
    output["rssi_dbm"] = observation.rssi;
    output["rssi_filtered_dbm"] = observation.filteredRssi;
    output["sample_count"] = observation.samples;
    output["age_ms"] = millis() - observation.lastSeen;
    output["observed_uptime_ms"] = observation.lastSeen;

    float reference = bluetooth ? config.bleReference : config.wifiReference;
    bool calibrated = bluetooth ? config.bleCalibrated : config.wifiCalibrated;
    const char *calibrationSource = calibrated ? "user_1m_reference" : "configured_assumption";
    if (bluetooth && observation.haveIBeacon && !config.bleCalibrated) {
        reference = observation.iBeacon.referenceAtOneMeter;
        calibrated = true;
        calibrationSource = "ibeacon_advertised_1m_reference";
    }
    output["reference_rssi_1m_dbm"] = reference;
    output["path_loss_exponent"] = config.exponent;
    output["calibrated"] = calibrated;
    output["calibration_source"] = calibrationSource;
    output["distance_method"] = calibrated ? "rssi_calibrated" : "rssi_assumed";
    const float distance = radio_telemetry_model::distanceMeters(observation.filteredRssi, reference, config.exponent);
    if (std::isfinite(distance)) output["distance_m"] = distance;
    else output["distance_m"] = nullptr;
    if (bluetooth) {
        output["address_type"] = observation.addressType;
        if (observation.haveTxPower) output["tx_power_dbm"] = observation.txPower;
        else output["tx_power_dbm"] = nullptr;
        if (observation.haveIBeacon) {
            char uuid[37];
            const uint8_t *u = observation.iBeacon.uuid;
            snprintf(uuid, sizeof(uuid), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                     u[0],u[1],u[2],u[3],u[4],u[5],u[6],u[7],u[8],u[9],u[10],u[11],u[12],u[13],u[14],u[15]);
            JsonObject beacon = output.createNestedObject("ibeacon");
            beacon["uuid"] = String(uuid);
            beacon["major"] = observation.iBeacon.major;
            beacon["minor"] = observation.iBeacon.minor;
            beacon["reference_rssi_1m_dbm"] = observation.iBeacon.referenceAtOneMeter;
        } else output["ibeacon"] = nullptr;
    } else {
        output["channel"] = observation.channel;
        output["ftm_responder"] = observation.ftmResponder;
    }
}

void putEnvelope(JsonObject output, const char *type, uint32_t sequence) {
    output["schema"] = 1;
    if (type) output["type"] = type;
    output["receiver_id"] = receiverId;
    output["boot_id"] = bootId;
    output["uptime_ms"] = millis();
    output["sequence"] = sequence;
    timeval clock;
    gettimeofday(&clock, nullptr);
    if (clock.tv_sec >= 1700000000) {
        output["timestamp_ms"] = static_cast<uint64_t>(clock.tv_sec) * 1000ULL + clock.tv_usec / 1000;
    } else output["timestamp_ms"] = nullptr;
}

void putFtm(JsonObject output, const FtmState &copy) {
    output["supported"] = static_cast<bool>(RADIO_HAS_FTM);
    output["busy"] = copy.busy;
    output["retiring"] = copy.retiring;
    output["status"] = String(copy.status);
    output["cancel_error"] = copy.cancelError;
    if (copy.retiring) output["recovery"] = "Wait for cancellation report; restart device if it never arrives";
    else output["recovery"] = nullptr;
    if (copy.channel) {
        char address[18];
        formatMac(copy.target, address);
        output["target"] = String(address);
        output["channel"] = copy.channel;
    } else output["target"] = nullptr;
    output["distance_method"] = "wifi_ftm";
    if (copy.hasReport) output["age_ms"] = millis() - copy.completed;
    else output["age_ms"] = nullptr;
    if (copy.successful && copy.hasReport) {
        output["rtt_ns"] = copy.rtt;
        output["distance_m"] = copy.distanceCm / 100.0;
        output["samples"] = copy.samples;
    } else {
        output["rtt_ns"] = nullptr;
        output["distance_m"] = nullptr;
        output["samples"] = 0;
    }
}

void putSummary(JsonObject output, bool withConfig) {
    Config config;
    Status state;
    FtmState ftmCopy;
    bool paused;
    uint32_t sequence;
    size_t wifiCount = 0, bleCount = 0;
    {
        Lock lock;
        config = configuration;
        state = status;
        ftmCopy = ftm;
        paused = isPaused;
        sequence = packetSequence;
        for (const auto &observation : wifiObservations) if (observation.used) ++wifiCount;
        for (const auto &observation : bleObservations) if (observation.used) ++bleCount;
    }
    putEnvelope(output, withConfig ? nullptr : "summary", sequence);
    output["paused"] = paused;
    output["observation_ttl_ms"] = OBSERVATION_TTL_MS;
    JsonObject capabilities = output.createNestedObject("capabilities");
    capabilities["wifi"] = true;
    capabilities["ble"] = static_cast<bool>(RADIO_HAS_BLE);
    capabilities["ftm"] = static_cast<bool>(RADIO_HAS_FTM);
    capabilities["bluetooth_classic"] = false;
    capabilities["ahrs"] = true;
    capabilities["ultrasonic"] = true;
    JsonObject counts = output.createNestedObject("counts");
    counts["wifi"] = wifiCount;
    counts["ble"] = bleCount;
    JsonObject report = output.createNestedObject("status");
    report["worker_ready"] = state.workerReady;
    report["wifi_scan"] = state.wifiScan;
    report["wifi_scan_mode"] = "passive_interleaved_channels";
    report["wifi_scan_result_scope"] = "single_channel";
    report["wifi_scan_channel"] = state.wifiScanChannel;
    report["wifi_scan_dwell_ms"] = static_cast<uint32_t>(radio_telemetry_model::PassiveWifiSweep::DWELL_MS);
    report["wifi_channel_spacing_ms"] = static_cast<uint32_t>(radio_telemetry_model::PassiveWifiSweep::SPACING_MS);
    report["ble_scan"] = state.bleScan;
    report["ble_initialized"] = state.bleInitialized;
    report["ble_initializing"] = state.bleInitializing;
    if (state.wifiScanned) report["wifi_last_scan_age_ms"] = millis() - state.wifiLastScan;
    else report["wifi_last_scan_age_ms"] = nullptr;
    if (state.bleScanned) report["ble_last_scan_age_ms"] = millis() - state.bleLastScan;
    else report["ble_last_scan_age_ms"] = nullptr;
    report["wifi_error"] = state.wifiError;
    report["ble_error"] = state.bleError;
    report["ble_scan_end_reason"] = state.bleEndReason;
    report["udp_sent"] = state.udpSent;
    report["udp_failed"] = state.udpFailed;
    report["wifi_evictions"] = state.wifiEvictions;
    report["ble_evictions"] = state.bleEvictions;
    report["wifi_truncated"] = state.wifiTruncated;
    if (withConfig) {
        putConfig(output.createNestedObject("config"), config);
        ahrs_telemetry::writeJson(output.createNestedObject("ahrs"));
        ultrasonic_telemetry::writeJson(output.createNestedObject("ultrasonic"));
    }
    putFtm(output.createNestedObject("ftm"), ftmCopy);
}

#if RADIO_HAS_BLE
class ScanCallbacks final : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice *device) override {
        uint8_t address[6];
        const std::string addressText = device->getAddress().toString();
        if (!radio_telemetry_model::parseMac(addressText.c_str(), address)) return;
        const int8_t rssi = device->getRSSI();
        if (rssi < -127 || rssi > 20) return; // reject unavailable/invalid controller RSSI
        const std::string name = device->haveName() ? device->getName() : std::string();
        const bool haveTx = device->haveTXPower();
        const int8_t tx = haveTx ? device->getTXPower() : 0;
        IBeacon beacon = {};
        bool isBeacon = false;
        if (device->haveManufacturerData()) {
            const std::string manufacturer = device->getManufacturerData();
            isBeacon = radio_telemetry_model::decodeIBeacon(
                reinterpret_cast<const uint8_t *>(manufacturer.data()), manufacturer.size(), beacon);
        }
        const uint32_t now = millis();
        Lock lock;
        if (isPaused || !configuration.bleEnabled) return;
        Observation &observation = slotFor(bleObservations, BLE_CAPACITY, address,
                                           device->getAddressType(), now, status.bleEvictions);
        observeRssi(observation, rssi, now);
        if (!name.empty())
            copyName(observation.name, reinterpret_cast<const uint8_t *>(name.data()), name.size());
        if (haveTx) {
            observation.haveTxPower = true;
            observation.txPower = tx;
        }
        if (isBeacon) {
            observation.haveIBeacon = true;
            observation.iBeacon = beacon;
        }
    }

    void onScanEnd(const NimBLEScanResults &, int reason) override {
        Lock lock;
        status.bleScan = false;
        status.bleScanned = true;
        status.bleLastScan = millis();
        status.bleEndReason = reason;
    }
};
ScanCallbacks bleCallbacks;

NimBLEScan *initializeBluetooth() {
    if (!NimBLEDevice::init("")) return nullptr;
    NimBLEScan *scan = NimBLEDevice::getScan();
    if (!scan) return nullptr;
    scan->setScanCallbacks(&bleCallbacks, true);
    scan->setActiveScan(false);
    scan->setDuplicateFilter(0);
    scan->setMaxResults(0);
    scan->setInterval(300);
    scan->setWindow(30);
    return scan;
}

void bluetoothInitializer(void *) {
    // NimBLE init can wait indefinitely for controller/host sync. Keep that wait
    // out of the radio worker so Wi-Fi, publication and OTA pause still progress.
    NimBLEScan *scan = initializeBluetooth();
    {
        Lock lock;
        readyBleScan = scan;
        status.bleInitialized = scan != nullptr;
        status.bleInitializing = false;
        status.bleError = scan ? 0 : -1;
    }
    vTaskDelete(nullptr);
}

void requestBluetoothInitialization() {
    {
        Lock lock;
        if (isPaused || status.bleInitializing || status.bleInitialized) return;
        status.bleInitializing = true;
        status.bleError = 0;
    }
    if (xTaskCreate(bluetoothInitializer, "radio-ble-init", 4096, nullptr, 1, nullptr) != pdPASS) {
        Lock lock;
        status.bleInitializing = false;
        status.bleError = -3;
    }
}
#endif

#if RADIO_HAS_FTM
const char *ftmStatusName(wifi_ftm_status_t value) {
    switch (value) {
        case FTM_STATUS_SUCCESS: return "success";
        case FTM_STATUS_UNSUPPORTED: return "unsupported";
        case FTM_STATUS_CONF_REJECTED: return "configuration_rejected";
        case FTM_STATUS_NO_RESPONSE: return "no_response";
        default: return "failed";
    }
}

void onFtmReport(arduino_event_t *event) {
    const wifi_event_ftm_report_t &report = event->event_info.wifi_ftm_report;
    {
        Lock lock;
        if (ftm.busy && !ftm.pending && !memcmp(ftm.target, report.peer_mac, 6)) {
            ftm.busy = false;
            ftm.hasReport = true;
            ftm.completed = millis();
            if (ftm.retiring) {
                // A cancelled request must drain before another session can start.
                // The SDK has no request ID, so MAC matching alone cannot correlate
                // a late report with a second request to the same responder.
                ftm.retiring = false;
                ftm.successful = false;
                ftm.rtt = 0;
                ftm.distanceCm = 0;
                ftm.samples = 0;
                snprintf(ftm.status, sizeof(ftm.status), "%s", ftm.stopReason);
            } else {
                ftm.successful = report.status == FTM_STATUS_SUCCESS;
                ftm.rtt = report.rtt_est;
                ftm.distanceCm = report.dist_est;
                ftm.samples = report.ftm_report_num_entries;
                snprintf(ftm.status, sizeof(ftm.status), "%s", ftmStatusName(report.status));
            }
        }
    }
    // Ownership follows Arduino's bundled FTM example: release report entries.
    free(report.ftm_report_data);
}
#endif

void collectWifiResults(int count) {
    const uint32_t now = millis();
    for (int index = 0; index < count && index < static_cast<int>(WIFI_CAPACITY); ++index) {
        const wifi_ap_record_t *record =
            static_cast<const wifi_ap_record_t *>(WiFi.getScanInfoByIndex(index));
        if (!record || record->rssi < -127 || record->rssi > 20) continue;
        Lock lock;
        if (isPaused || !configuration.wifiEnabled) break;
        Observation &observation = slotFor(wifiObservations, WIFI_CAPACITY, record->bssid, 0,
                                           now, status.wifiEvictions);
        observeRssi(observation, record->rssi, now);
        observation.channel = record->primary;
        observation.ftmResponder = record->ftm_responder;
        copyName(observation.name, record->ssid, strnlen(reinterpret_cast<const char *>(record->ssid), 32));
    }
    Lock lock;
    if (count > static_cast<int>(WIFI_CAPACITY))
        status.wifiTruncated += count - WIFI_CAPACITY;
    status.wifiScanned = true;
    status.wifiLastScan = now;
    status.wifiError = 0;
}

void expireObservations(uint32_t now) {
    Lock lock;
    for (auto &observation : wifiObservations)
        if (observation.used && now - observation.lastSeen > OBSERVATION_TTL_MS) observation.used = false;
    for (auto &observation : bleObservations)
        if (observation.used && now - observation.lastSeen > OBSERVATION_TTL_MS) observation.used = false;
}

void cancelFtm(const char *reason, uint32_t now) {
    bool wasRunning;
    uint32_t generation;
    {
        Lock lock;
        if (!ftm.busy || ftm.retiring) return;
        wasRunning = !ftm.pending;
        generation = ftm.generation;
        ftm.pending = false;
        ftm.hasReport = true;
        ftm.successful = false;
        ftm.completed = now;
        ftm.retiring = wasRunning;
        ftm.retiredAt = now;
        ftm.busy = wasRunning;
        snprintf(ftm.stopReason, sizeof(ftm.stopReason), "%s", reason);
        snprintf(ftm.status, sizeof(ftm.status), "%s", wasRunning ? "cancelling" : reason);
    }
#if RADIO_HAS_FTM
    if (wasRunning) {
        const esp_err_t result = esp_wifi_ftm_end_session();
        Lock lock;
        if (ftm.generation == generation) ftm.cancelError = result;
    }
#else
    (void)wasRunning;
    (void)generation;
#endif
}

void startPendingFtm(uint32_t now) {
#if RADIO_HAS_FTM
    FtmState requested;
    {
        Lock lock;
        if (!ftm.pending || isPaused) return;
        ftm.pending = false;
        ftm.started = now;
        snprintf(ftm.status, sizeof(ftm.status), "measuring");
        requested = ftm;
    }
    if (!WiFi.initiateFTM(16, 2, requested.channel, requested.target)) {
        Lock lock;
        if (ftm.generation != requested.generation) return;
        ftm.busy = false;
        ftm.hasReport = true;
        ftm.successful = false;
        ftm.completed = millis();
        snprintf(ftm.status, sizeof(ftm.status), "start_failed");
    }
#else
    (void)now;
#endif
}

bool sendDatagram(WiFiUDP &udp, IPAddress target, uint16_t port, const JsonDocument &document) {
    if (pauseRequested()) return false;
    char packet[1200];
    bool sent = false;
    const size_t size = measureJson(document);
    if (!document.overflowed() && size > 0 && size < sizeof(packet)) {
        size_t written = serializeJson(document, packet, sizeof(packet));
        if (written == size && udp.beginPacket(target, port) == 1) {
            sent = udp.write(reinterpret_cast<const uint8_t *>(packet), written) == written &&
                   udp.endPacket() == 1;
            if (!sent) udp.stop();
        }
    }
    Lock lock;
    if (sent) ++status.udpSent;
    else ++status.udpFailed;
    return sent;
}

// Publishes at most one radio-table datagram per worker iteration, with no backlog.
void publishStep(WiFiUDP &udp, const Config &config, uint32_t now,
                 bool &publishing, size_t &cursor, uint32_t &lastStart) {
    if (pauseRequested() || !config.udpHost[0]) { publishing = false; return; }
    IPAddress destination;
    if (!destination.fromString(config.udpHost)) { publishing = false; return; }
    if (!publishing) {
        if (now - lastStart < config.publishInterval) return;
        lastStart = now;
        cursor = 0;
        publishing = true;
        {
            Lock lock;
            ++packetSequence;
        }
        // Summary is kept deliberately small enough for one nonfragmented datagram.
        DynamicJsonDocument summary(3072);
        putSummary(summary.to<JsonObject>(), false);
        sendDatagram(udp, destination, config.udpPort, summary);
        return;
    }
    Observation observation;
    while (cursor < WIFI_CAPACITY + BLE_CAPACITY) {
        const bool bluetooth = cursor >= WIFI_CAPACITY;
        const size_t index = bluetooth ? cursor - WIFI_CAPACITY : cursor;
        ++cursor;
        if (!copyObservation(bluetooth, index, observation)) continue;
        if (now - observation.lastSeen > OBSERVATION_TTL_MS) continue;
        DynamicJsonDocument document(2048);
        JsonObject output = document.to<JsonObject>();
        uint32_t sequence;
        {
            Lock lock;
            sequence = ++packetSequence;
        }
        putEnvelope(output, "observation", sequence);
        output["radio"] = bluetooth ? "ble" : "wifi";
        putObservation(output.createNestedObject("observation"), observation, bluetooth, config);
        sendDatagram(udp, destination, config.udpPort, document);
        return;
    }
    publishing = false;
}

// AHRS has its own cadence; radio table publication never queues motion samples.
void publishAhrs(WiFiUDP &udp, const Config &config, uint32_t now, uint32_t &lastPublish) {
    if (pauseRequested() || !config.udpHost[0] ||
        now - lastPublish < config.ahrsPublishInterval) return;
    lastPublish = now;
    IPAddress destination;
    if (!destination.fromString(config.udpHost)) return;
    DynamicJsonDocument document(4096);
    JsonObject output = document.to<JsonObject>();
    uint32_t sequence;
    {
        Lock lock;
        sequence = ++packetSequence;
    }
    putEnvelope(output, "ahrs", sequence);
    ahrs_telemetry::writeJson(output.createNestedObject("ahrs"), true);
    sendDatagram(udp, destination, config.udpPort, document);
}

void publishUltrasonic(WiFiUDP &udp, const Config &config, uint32_t now, uint32_t &lastPublish) {
    if (pauseRequested() || !config.udpHost[0] || now - lastPublish < 260) return;
    lastPublish = now;
    IPAddress destination;
    if (!destination.fromString(config.udpHost)) return;
    DynamicJsonDocument document(6144);
    JsonObject output = document.to<JsonObject>();
    uint32_t sequence;
    { Lock lock; sequence = ++packetSequence; }
    putEnvelope(output, "ultrasonic", sequence);
    ultrasonic_telemetry::writeJson(output.createNestedObject("ultrasonic"), true);
    sendDatagram(udp, destination, config.udpPort, document);
}

void worker(void *) {
    WiFiUDP udp;
    uint32_t lastUltrasonicPublish = 0;
    bool wifiOwned = false, wifiCancelled = false, publishing = false;
    uint32_t wifiStarted = 0;
    radio_telemetry_model::PassiveWifiSweep wifiSweep;
    uint32_t nextWifi = millis() + 5000;
    uint32_t nextBle = millis() + 1000;
    uint32_t lastExpire = millis(), lastPublish = millis(), lastRevision = 0;
    uint32_t lastAhrsPublish = millis();
    size_t publicationCursor = 0;
#if RADIO_HAS_BLE
    NimBLEScan *bleScan = nullptr;
#endif
    {
        Lock lock;
        status.workerReady = true;
    }
    for (;;) {
        const uint32_t now = millis();
        Config config;
        bool paused;
        uint32_t revision;
        FtmState ftmCopy;
        {
            Lock lock;
            config = configuration;
            paused = isPaused;
            revision = configurationRevision;
            ftmCopy = ftm;
        }
        if (revision != lastRevision) {
            publishing = false;
            lastPublish = now - config.publishInterval;
            lastAhrsPublish = now - config.ahrsPublishInterval;
            lastRevision = revision;
            wifiSweep.reset();
            nextWifi = now + 1000;
            nextBle = now + 1000;
        }
        if (now - lastExpire >= 1000) {
            expireObservations(now);
            lastExpire = now;
        }
#if RADIO_HAS_BLE
        if (!bleScan) {
            Lock lock;
            if (readyBleScan) {
                bleScan = readyBleScan;
                nextBle = now;
            }
        }
#endif
        const bool connected = WiFi.status() == WL_CONNECTED;
        if (wifiOwned) {
            int count = WiFi.scanComplete();
            if (count >= 0) {
                if (!wifiCancelled && !paused && connected) collectWifiResults(count);
                WiFi.scanDelete();
                wifiOwned = false;
                wifiCancelled = false;
                Lock lock;
                status.wifiScan = false;
            } else if (count == WIFI_SCAN_FAILED || now - wifiStarted > WIFI_SCAN_TIMEOUT_MS) {
                esp_wifi_scan_stop();
                WiFi.scanDelete();
                wifiOwned = false;
                wifiCancelled = false;
                Lock lock;
                status.wifiScan = false;
                status.wifiError = count == WIFI_SCAN_FAILED ? WIFI_SCAN_FAILED : -3;
            }
        }
        if (paused || !connected) {
            wifiSweep.reset();
            if (wifiOwned && !wifiCancelled) {
                esp_wifi_scan_stop();
                wifiCancelled = true;
            }
#if RADIO_HAS_BLE
            if (bleScan && bleScan->isScanning()) {
                bleScan->stop();
                Lock lock;
                status.bleScan = false;
            }
#endif
            cancelFtm(paused ? "paused" : "disconnected", now);
            publishing = false;
            udp.stop();
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        if (wifiOwned && !config.wifiEnabled && !wifiCancelled) {
            esp_wifi_scan_stop();
            wifiCancelled = true;
        }
#if RADIO_HAS_BLE
        if (bleScan && !config.bleEnabled && bleScan->isScanning()) {
            bleScan->stop();
            Lock lock;
            status.bleScan = false;
        }
        bool bluetoothScanning = bleScan && bleScan->isScanning();
#else
        const bool bluetoothScanning = false;
#endif
        {
            Lock lock;
            ftmCopy = ftm;
        }
        if (ftmCopy.retiring && now - ftmCopy.retiredAt >= 2000) {
            Lock lock;
            if (ftm.retiring && ftm.generation == ftmCopy.generation)
                snprintf(ftm.status, sizeof(ftm.status), "retirement_stalled");
        }
        if (ftmCopy.busy && !ftmCopy.pending && !ftmCopy.retiring &&
            now - ftmCopy.started >= FTM_TIMEOUT_MS) {
            cancelFtm("timeout", now);
            Lock lock;
            ftmCopy = ftm;
        }
        // Waiting for a cancelled FTM report blocks only new FTM requests.
        // Wi-Fi/BLE scanning and telemetry delivery continue.
        const bool ftmActive = ftmCopy.busy && !ftmCopy.retiring;
        if (ftmCopy.pending && !wifiOwned && !bluetoothScanning) {
            startPendingFtm(now);
        } else if (!ftmActive && !wifiOwned && !bluetoothScanning &&
                   config.wifiEnabled && static_cast<int32_t>(now - nextWifi) >= 0) {
            // Arduino owns its scan result allocation. Do not replace another active scan.
            if (!pauseRequested() && WiFi.scanComplete() != WIFI_SCAN_RUNNING) {
                // Keep every passive channel dwell, interleaved with home-channel
                // traffic. A full all-channel sweep otherwise stalls camera TCP.
                if (!wifiSweep.active()) {
                    wifi_country_t country = {};
                    const esp_err_t error = esp_wifi_get_country(&country);
                    if (error != ESP_OK || !wifiSweep.begin(country.schan, country.nchan, now)) {
                        nextWifi = now + 1000;
                        Lock lock;
                        status.wifiError = error == ESP_OK ? ESP_ERR_INVALID_ARG : error;
                    }
                }
                if (wifiSweep.active()) {
                    const uint8_t channel = wifiSweep.channel();
                    int result = WiFi.scanNetworks(true, true, true,
                        radio_telemetry_model::PassiveWifiSweep::DWELL_MS, channel);
                    if (result == WIFI_SCAN_RUNNING) {
                        nextWifi = wifiSweep.advance(now, config.wifiInterval);
                        wifiOwned = true;
                        wifiCancelled = false;
                        wifiStarted = now;
                        Lock lock;
                        status.wifiScan = true;
                        status.wifiScanChannel = channel;
                        status.wifiError = 0;
                    } else {
                        nextWifi = now + 1000; // Retry this channel; do not skip coverage.
                        Lock lock;
                        status.wifiError = result;
                    }
                }
            } else nextWifi = now + 1000;
        }
#if RADIO_HAS_BLE
        else if (!ftmActive && !wifiOwned && !bluetoothScanning &&
                 config.bleEnabled && static_cast<int32_t>(now - nextBle) >= 0) {
            nextBle = now + config.bleInterval;
            if (!bleScan) requestBluetoothInitialization();
            if (bleScan && !pauseRequested()) {
                const bool started = bleScan->start(config.bleDuration, false, true);
                Lock lock;
                status.bleScan = started;
                status.bleError = started ? 0 : -2;
            } else nextBle = now + 30000;
        }
#endif
        publishAhrs(udp, config, now, lastAhrsPublish);
        publishUltrasonic(udp, config, now, lastUltrasonicPublish);
        publishStep(udp, config, now, publishing, publicationCursor, lastPublish);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

bool authorized() {
    if (!httpServer) return false;
    if (!authorizeRequest || !authorizeRequest()) {
        if (!authorizeRequest) httpServer->send(403, "application/json", "{\"error\":\"Authentication callback unavailable\"}");
        return false;
    }
    if (!stateMutex) {
        httpServer->send(503, "application/json", "{\"error\":\"Telemetry memory unavailable\"}");
        return false;
    }
    return true;
}

void sendError(int code, const String &message) {
    DynamicJsonDocument document(512);
    document["error"] = message;
    String serialized;
    serializeJson(document, serialized);
    httpServer->sendHeader("Cache-Control", "no-store");
    httpServer->send(code, "application/json", serialized);
}

void handleSnapshot() {
    if (!authorized()) return;
    DynamicJsonDocument document(12288);
    putSummary(document.to<JsonObject>(), true);
    if (document.overflowed()) { sendError(503, "Telemetry summary allocation failed"); return; }
    String serialized;
    serialized.reserve(12288);
    serializeJson(document, serialized);
    if (!serialized.length()) { sendError(503, "Telemetry serialization failed"); return; }
    serialized.remove(serialized.length() - 1);
    httpServer->sendHeader("Cache-Control", "no-store");
    httpServer->setContentLength(CONTENT_LENGTH_UNKNOWN);
    httpServer->send(200, "application/json", "");
    httpServer->sendContent(serialized);
    Config config;
    {
        Lock lock;
        config = configuration;
    }
    for (int radio = 0; radio < 2; ++radio) {
        httpServer->sendContent(radio ? ",\"ble\":[" : ",\"wifi\":[");
        const size_t capacity = radio ? BLE_CAPACITY : WIFI_CAPACITY;
        bool first = true;
        for (size_t i = 0; i < capacity && httpServer->client().connected(); ++i) {
            Observation observation;
            if (!copyObservation(radio != 0, i, observation)) continue;
            document.clear();
            putObservation(document.to<JsonObject>(), observation, radio != 0, config);
            serialized = "";
            serializeJson(document, serialized);
            if (!first) httpServer->sendContent(",");
            httpServer->sendContent(serialized);
            first = false;
        }
        httpServer->sendContent("]");
    }
    httpServer->sendContent("}");
    httpServer->sendContent("");
}

void handleGetConfig() {
    if (!authorized()) return;
    Config config;
    {
        Lock lock;
        config = configuration;
    }
    DynamicJsonDocument document(2048);
    putConfig(document.to<JsonObject>(), config);
    String serialized;
    serializeJson(document, serialized);
    httpServer->sendHeader("Cache-Control", "no-store");
    httpServer->send(200, "application/json", serialized);
}

bool requireJsonContentType() {
    String type = httpServer->header("Content-Type");
    const int separator = type.indexOf(';');
    if (separator >= 0) type.remove(separator);
    type.trim();
    type.toLowerCase();
    if (type != "application/json") {
        sendError(415, "Content-Type must be application/json");
        return false;
    }
    return true;
}

void handleAhrs() {
    if (!authorized()) return;
    DynamicJsonDocument document(6144);
    JsonObject output = document.to<JsonObject>();
    uint32_t sequence;
    {
        Lock lock;
        sequence = packetSequence;
    }
    putEnvelope(output, "ahrs", sequence);
    ahrs_telemetry::writeJson(output.createNestedObject("ahrs"));
    if (document.overflowed()) { sendError(503, "AHRS snapshot allocation failed"); return; }
    String serialized;
    serializeJson(document, serialized);
    httpServer->sendHeader("Cache-Control", "no-store");
    httpServer->send(200, "application/json", serialized);
}

void handleUltrasonic() {
    if (!authorized()) return;
    DynamicJsonDocument document(6144);
    JsonObject output = document.to<JsonObject>();
    uint32_t sequence;
    { Lock lock; sequence = packetSequence; }
    putEnvelope(output, "ultrasonic", sequence);
    ultrasonic_telemetry::writeJson(output.createNestedObject("ultrasonic"));
    if (document.overflowed()) { sendError(503, "Ultrasonic allocation failed"); return; }
    String serialized;
    serializeJson(document, serialized);
    httpServer->sendHeader("Cache-Control", "no-store");
    httpServer->send(200, "application/json", serialized);
}

void handleAhrsCalibration() {
    if (!authorized() || !requireJsonContentType()) return;
    // A body-free command endpoint avoids ambiguous partial calibration settings.
    // A JSON empty object is accepted for conventional API clients.
    String body = httpServer->arg("plain");
    if (body.length() > 256) { sendError(413, "Calibration request body is too large"); return; }
    body.replace(" ", ""); body.replace("\t", ""); body.replace("\r", ""); body.replace("\n", "");
    if (body.length() && body != "{}") {
        sendError(400, "Send an empty body or {} to request stationary gyro calibration");
        return;
    }
    const auto result = ahrs_telemetry::requestGyroCalibration();
    if (result == ahrs_telemetry::CalibrationRequest::Busy) {
        sendError(409, "Gyro calibration is already pending");
        return;
    }
    if (result != ahrs_telemetry::CalibrationRequest::Accepted) {
        sendError(503, "A fresh, running IMU is required for gyro calibration");
        return;
    }
    httpServer->sendHeader("Cache-Control", "no-store");
    httpServer->send(202, "application/json", "{\"status\":\"queued\"}");
}

void handleSetConfig() {
    if (!authorized() || !requireJsonContentType()) return;
    const String body = httpServer->arg("plain");
    if (!body.length() || body.length() > 2048) {
        sendError(body.length() ? 413 : 400, "Provide a JSON configuration object of at most 2048 bytes");
        return;
    }
    DynamicJsonDocument document(3072);
    if (deserializeJson(document, body) || !document.is<JsonObject>()) {
        sendError(400, "Invalid JSON configuration object");
        return;
    }
    Config candidate;
    {
        Lock lock;
        candidate = configuration;
    }
    String error;
    if (!patchConfig(document.as<JsonObjectConst>(), candidate, error)) {
        sendError(400, error);
        return;
    }
    if (!saveConfig(candidate)) {
        sendError(500, "Could not persist telemetry configuration");
        return;
    }
    {
        Lock lock;
        configuration = candidate;
        ++configurationRevision;
    }
    handleGetConfig();
}

void handleFtm() {
    if (!authorized() || !requireJsonContentType()) return;
#if !RADIO_HAS_FTM
    sendError(501, "Wi-Fi FTM is not supported by this chip and SDK build");
#else
    const String body = httpServer->arg("plain");
    if (!body.length() || body.length() > 256) {
        sendError(body.length() ? 413 : 400, "Provide address and channel as a JSON object");
        return;
    }
    DynamicJsonDocument document(512);
    if (deserializeJson(document, body) || !document.is<JsonObject>()) {
        sendError(400, "Invalid FTM JSON");
        return;
    }
    JsonObjectConst object = document.as<JsonObjectConst>();
    for (JsonPairConst item : object) {
        if (strcmp(item.key().c_str(), "address") && strcmp(item.key().c_str(), "channel")) {
            sendError(400, "FTM accepts only address and channel");
            return;
        }
    }
    uint8_t target[6];
    const char *address = object["address"].as<const char *>();
    uint32_t channel = 0;
    if (!radio_telemetry_model::parseMac(address, target) ||
        !radio_telemetry_model::isUnicastTarget(target) ||
        !readUnsigned(object["channel"], channel, 1, 14)) {
        sendError(400, "FTM requires a unicast MAC address and channel from 1 to 14");
        return;
    }
    if (WiFi.status() != WL_CONNECTED) {
        sendError(503, "FTM requires a connected Wi-Fi station");
        return;
    }
    int failure = 0;
    {
        Lock lock;
        if (isPaused || !status.workerReady) failure = 503;
        else if (ftm.busy) failure = 409;
        else {
            ftm = FtmState();
            ftm.generation = ++ftmGeneration;
            ftm.pending = true;
            ftm.busy = true;
            memcpy(ftm.target, target, 6);
            ftm.channel = static_cast<uint8_t>(channel);
            snprintf(ftm.status, sizeof(ftm.status), "queued");
        }
    }
    if (failure) {
        sendError(failure, failure == 409 ? "An FTM measurement is already pending" : "Telemetry is paused or unavailable");
        return;
    }
    httpServer->sendHeader("Cache-Control", "no-store");
    httpServer->send(202, "application/json", "{\"status\":\"queued\"}");
#endif
}
} // namespace

bool writeFrameJson(JsonObject output, uint64_t captureUs) {
    if (!stateMutex) return false;
    uint32_t sequence;
    Config config;
    { Lock lock; sequence=packetSequence;config=configuration; }
    output["schema"]="biome.capture.v1";
    output["receiver_id"]=receiverId;output["boot_id"]=bootId;
    output["capture_monotonic_us"]=captureUs;output["capture_clock"]="esp_timer";
    char identity[64];snprintf(identity,sizeof(identity),"%s:%llu",bootId,static_cast<unsigned long long>(captureUs));output["frame_id"]=String(identity);
    JsonObject ahrs=output.createNestedObject("ahrs");
    putEnvelope(ahrs,"ahrs",sequence);ahrs["uptime_ms"]=static_cast<uint32_t>(captureUs/1000);
    if (!ahrs_telemetry::writeJsonAt(ahrs.createNestedObject("ahrs"),captureUs)) output["ahrs"]=nullptr;
    JsonObject sonar=output.createNestedObject("ultrasonic");
    putEnvelope(sonar,"ultrasonic",sequence);
    sonar["uptime_ms"]=static_cast<uint32_t>(captureUs/1000);
    ultrasonic_telemetry::writeJsonAt(sonar.createNestedObject("ultrasonic"),captureUs);
    JsonObject radio=output.createNestedObject("radio");
    putEnvelope(radio,nullptr,sequence);radio["uptime_ms"]=static_cast<uint32_t>(captureUs/1000);
    for (int kind=0;kind<2;++kind) {
        JsonArray rows=radio.createNestedArray(kind ? "ble" : "wifi");
        const size_t count=kind ? BLE_CAPACITY : WIFI_CAPACITY;
        for(size_t i=0;i<count;++i) {
            Observation observation;
            if (!copyObservation(kind!=0,i,observation)) continue;
            const uint32_t age=static_cast<uint32_t>(captureUs/1000)-observation.lastSeen;
            if (age>=0x80000000UL) continue; // A later scan cannot be assigned to this frame.
            JsonObject row=rows.createNestedObject();
            putObservation(row,observation,kind!=0,config);row["age_ms"]=age;
        }
    }
    return true;
}

void begin(WebServer &server, bool (*authorize)()) {
    if (httpServer) return;
    httpServer = &server;
    authorizeRequest = authorize;
    // WebServer always retains Authorization in addition to these headers.
    const char *headers[] = {"Content-Type"};
    server.collectHeaders(headers, 1);
    stateMutex = xSemaphoreCreateMutex();
    snprintf(receiverId, sizeof(receiverId), "esp32cam-%012llx",
             static_cast<unsigned long long>(ESP.getEfuseMac()));
    snprintf(bootId, sizeof(bootId), "%08lx%08lx",
             static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));
    server.on("/api/telemetry", HTTP_GET, handleSnapshot);
    server.on("/api/telemetry/config", HTTP_GET, handleGetConfig);
    server.on("/api/telemetry/config", HTTP_POST, handleSetConfig);
    server.on("/api/telemetry/ftm", HTTP_POST, handleFtm);
    server.on("/api/ahrs", HTTP_GET, handleAhrs);
    server.on("/api/ahrs/calibrate", HTTP_POST, handleAhrsCalibration);
    ahrs_telemetry::begin();
    ultrasonic_telemetry::begin();
    server.on("/api/ultrasonic", HTTP_GET, handleUltrasonic);
    if (!stateMutex) return;
    loadConfig();
#if RADIO_HAS_FTM
    WiFi.onEvent(onFtmReport, ARDUINO_EVENT_WIFI_FTM_REPORT);
#else
    snprintf(ftm.status, sizeof(ftm.status), "unsupported");
#endif
    if (xTaskCreate(worker, "radio-telemetry", 6144, nullptr, 1, &workerHandle) != pdPASS) {
        workerHandle = nullptr;
        status.workerReady = false;
    }
}

void loop() {
    // Radio work belongs to its own low-priority task, never the camera service loop.
}

void pause(bool paused) {
    ahrs_telemetry::pause(paused);
    ultrasonic_telemetry::pause(paused);
    if (!stateMutex) return;
    Lock lock;
    isPaused = paused;
}
} // namespace radio_telemetry

