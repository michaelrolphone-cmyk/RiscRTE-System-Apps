#include "radio_telemetry.h"
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

#if defined(CONFIG_ESP_WIFI_FTM_INITIATOR_SUPPORT) && CONFIG_ESP_WIFI_FTM_INITIATOR_SUPPORT
#define RADIO_HAS_FTM 1
#else
#define RADIO_HAS_FTM 0
#endif

namespace radio_telemetry {
namespace {
constexpr uint32_t FTM_TIMEOUT_MS = 12000;

struct Config {
    uint32_t publishInterval = 5000;
    uint32_t ahrsPublishInterval = 100;
    char udpHost[16] = {};
    uint16_t udpPort = 9876;
};
struct Status {
    bool workerReady = false;
    uint32_t udpSent = 0;
    uint32_t udpFailed = 0;
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

void putConfig(JsonObject output, const Config &config) {
    output["publish_interval_ms"] = config.publishInterval;
    output["ahrs_publish_interval_ms"] = config.ahrsPublishInterval;
    output["udp_host"] = String(config.udpHost);
    output["udp_port"] = config.udpPort;
}

bool readUnsigned(JsonVariantConst value, uint32_t &target, uint32_t low, uint32_t high) {
    if (!value.is<uint32_t>()) return false;
    uint32_t result = value.as<uint32_t>();
    if (result < low || result > high) return false;
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
        if (!strcmp(key, "publish_interval_ms")) valid = readUnsigned(value, config.publishInterval, 1000, 86400000);
        else if (!strcmp(key, "ahrs_publish_interval_ms")) valid = readUnsigned(value, config.ahrsPublishInterval, 50, 86400000);
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
    // Preserve sensor delivery settings while retiring saved scan/calibration keys.
    for (const char *key : {"wifi_enabled", "ble_enabled", "wifi_interval_ms",
            "ble_interval_ms", "ble_duration_ms", "wifi_reference_rssi_1m_dbm",
            "ble_reference_rssi_1m_dbm", "path_loss_exponent", "wifi_calibrated",
            "ble_calibrated"}) document.remove(key);
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
    Config config; Status state; FtmState ftmCopy; bool paused; uint32_t sequence;
    {
        Lock lock;
        config=configuration; state=status; ftmCopy=ftm;
        paused=isPaused; sequence=packetSequence;
    }
    putEnvelope(output, withConfig ? nullptr : "summary", sequence);
    output["paused"]=paused;
    JsonObject capabilities=output.createNestedObject("capabilities");
    capabilities["ftm"]=static_cast<bool>(RADIO_HAS_FTM);
    capabilities["ahrs"]=true;
    capabilities["ultrasonic"]=true;
    JsonObject report=output.createNestedObject("status");
    report["worker_ready"]=state.workerReady;
    report["udp_sent"]=state.udpSent;
    report["udp_failed"]=state.udpFailed;
    if (withConfig) {
        putConfig(output.createNestedObject("config"),config);
        ahrs_telemetry::writeJson(output.createNestedObject("ahrs"));
        ultrasonic_telemetry::writeJson(output.createNestedObject("ultrasonic"));
    }
    putFtm(output.createNestedObject("ftm"),ftmCopy);
}

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

// Publish the latest status without retaining scan observations or a backlog.
void publishSummary(WiFiUDP &udp,const Config &config,uint32_t now,uint32_t &lastStart) {
    if (pauseRequested() || !config.udpHost[0] || now-lastStart<config.publishInterval) return;
    lastStart=now;
    IPAddress destination;
    if (!destination.fromString(config.udpHost)) return;
    { Lock lock; ++packetSequence; }
    DynamicJsonDocument summary(3072);
    putSummary(summary.to<JsonObject>(),false);
    sendDatagram(udp,destination,config.udpPort,summary);
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
    uint32_t lastAhrsPublish=millis(),lastUltrasonicPublish=0,lastPublish=millis();
    { Lock lock; status.workerReady=true; }
    for (;;) {
        const uint32_t now=millis();
        Config config; FtmState ftmCopy; bool paused;
        { Lock lock; config=configuration;ftmCopy=ftm;paused=isPaused; }
        if (paused || WiFi.status()!=WL_CONNECTED) {
            cancelFtm(paused ? "paused" : "disconnected",now);
            udp.stop();
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        if (ftmCopy.retiring && now-ftmCopy.retiredAt>=2000) {
            Lock lock;
            if (ftm.retiring && ftm.generation==ftmCopy.generation)
                snprintf(ftm.status,sizeof(ftm.status),"retirement_stalled");
        }
        if (ftmCopy.busy && !ftmCopy.pending && !ftmCopy.retiring &&
                now-ftmCopy.started>=FTM_TIMEOUT_MS)
            cancelFtm("timeout",now);
        if (ftmCopy.pending) startPendingFtm(now);
        publishAhrs(udp,config,now,lastAhrsPublish);
        publishUltrasonic(udp,config,now,lastUltrasonicPublish);
        publishSummary(udp,config,now,lastPublish);
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
    putSummary(document.to<JsonObject>(),true);
    if (document.overflowed()) { sendError(503,"Telemetry summary allocation failed");return; }
    String serialized;
    serializeJson(document,serialized);
    httpServer->sendHeader("Cache-Control","no-store");
    httpServer->send(200,"application/json",serialized);
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

bool parseTarget(const char *address,uint8_t target[6]) {
    if (!address || strlen(address)!=17) return false;
    bool nonzero=false;
    for (int i=0;i<6;++i) {
        if (i && address[i*3-1]!=':') return false;
        unsigned value=0;
        for (int j=0;j<2;++j) {
            char c=address[i*3+j];
            unsigned digit;
            if (c>='0' && c<='9') digit=c-'0';
            else if (c>='a' && c<='f') digit=c-'a'+10;
            else if (c>='A' && c<='F') digit=c-'A'+10;
            else return false;
            value=(value<<4)|digit;
        }
        target[i]=value;nonzero|=value!=0;
    }
    return nonzero && !(target[0]&1);
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
    if (!parseTarget(address, target) ||
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
    { Lock lock; sequence=packetSequence; }
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
    FtmState ftmCopy;
    { Lock lock; ftmCopy=ftm; }
    if (!ftmCopy.hasReport || static_cast<uint32_t>(captureUs/1000)-ftmCopy.completed<0x80000000UL) {
        JsonObject range=output.createNestedObject("ftm");
        putFtm(range,ftmCopy);
        if (ftmCopy.hasReport) range["age_ms"]=static_cast<uint32_t>(captureUs/1000)-ftmCopy.completed;
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

