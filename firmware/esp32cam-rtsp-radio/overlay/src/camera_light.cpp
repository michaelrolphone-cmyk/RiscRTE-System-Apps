#include "camera_light.h"
#include "camera_light_model.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <driver/rmt.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <cstring>

#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "The camera light profile is for the ESP32-S3 CAM only."
#endif

namespace camera_light {
namespace {
using camera_light_model::State;
using camera_light_model::Rgb;
constexpr gpio_num_t LIGHT_GPIO = GPIO_NUM_48;
constexpr rmt_channel_t LIGHT_CHANNEL = RMT_CHANNEL_3;
static_assert(LIGHT_GPIO != GPIO_NUM_4, "GPIO4 is the S3 camera SCCB data pin, not this light.");
#define CHECK_CAMERA_PIN(pin) static_assert(static_cast<int>(LIGHT_GPIO) != static_cast<int>(pin), "Camera light pin conflicts with " #pin)
#ifdef CAMERA_CONFIG_PIN_PWDN
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_PWDN);
#endif
#ifdef CAMERA_CONFIG_PIN_RESET
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_RESET);
#endif
#ifdef CAMERA_CONFIG_PIN_XCLK
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_XCLK);
#endif
#ifdef CAMERA_CONFIG_PIN_SCCB_SDA
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_SCCB_SDA);
#endif
#ifdef CAMERA_CONFIG_PIN_SCCB_SCL
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_SCCB_SCL);
#endif
#ifdef CAMERA_CONFIG_PIN_Y2
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_Y2);
#endif
#ifdef CAMERA_CONFIG_PIN_Y3
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_Y3);
#endif
#ifdef CAMERA_CONFIG_PIN_Y4
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_Y4);
#endif
#ifdef CAMERA_CONFIG_PIN_Y5
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_Y5);
#endif
#ifdef CAMERA_CONFIG_PIN_Y6
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_Y6);
#endif
#ifdef CAMERA_CONFIG_PIN_Y7
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_Y7);
#endif
#ifdef CAMERA_CONFIG_PIN_Y8
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_Y8);
#endif
#ifdef CAMERA_CONFIG_PIN_Y9
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_Y9);
#endif
#ifdef CAMERA_CONFIG_PIN_VSYNC
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_VSYNC);
#endif
#ifdef CAMERA_CONFIG_PIN_HREF
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_HREF);
#endif
#ifdef CAMERA_CONFIG_PIN_PCLK
CHECK_CAMERA_PIN(CAMERA_CONFIG_PIN_PCLK);
#endif
#undef CHECK_CAMERA_PIN

WebServer *httpServer = nullptr;
bool (*authorizeRequest)() = nullptr;
SemaphoreHandle_t commandMutex = nullptr;
State state;
Rgb lastTransmitted = {0, 0, 0};
bool driverInstalled = false;
bool driverReady = false;
bool commandKnown = false;
bool lastSucceeded = false;
bool haveSuccess = false;
bool haveAttempt = false;
uint32_t lastSuccessMs = 0;
uint32_t lastAttemptMs = 0;
uint32_t counterClockHz = 0;
esp_err_t lastError = ESP_OK;
const char *lastErrorStage = "";
// The IDF async write retains this pointer. Keep it alive even after a timeout.
rmt_item32_t transmitItems[camera_light_model::FRAME_ITEMS] = {};

struct Lock {
    Lock() { xSemaphoreTake(commandMutex, portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(commandMutex); }
};

esp_err_t fail(esp_err_t error, const char *stage) {
    lastError = error;
    lastErrorStage = stage;
    lastSucceeded = false;
    commandKnown = false;
    return error;
}

void retireDriver() {
    if (!driverInstalled) return;
    rmt_tx_stop(LIGHT_CHANNEL);
    // A failed/incomplete transaction must not leave a locked TX semaphore
    // for the next request. Only uninstall the channel this module installed.
    const esp_err_t result = rmt_driver_uninstall(LIGHT_CHANNEL);
    if (result == ESP_OK) driverInstalled = false;
    driverReady = false;
}

esp_err_t ensureDriver() {
    if (driverReady) return ESP_OK;
    if (driverInstalled) return fail(ESP_ERR_INVALID_STATE, "rmt_recovery");
    rmt_channel_status_result_t channels = {};
    esp_err_t result = rmt_get_channel_status(&channels);
    if (result != ESP_OK) return fail(result, "rmt_channel_status");
    if (channels.status[LIGHT_CHANNEL] != RMT_CHANNEL_UNINIT)
        return fail(ESP_ERR_INVALID_STATE, "rmt_channel_in_use");

    rmt_config_t config = RMT_DEFAULT_CONFIG_TX(LIGHT_GPIO, LIGHT_CHANNEL);
    // ESP32-S3 uses its stable XTAL source with AWARE_DFS. Measure the actual
    // counter rate below rather than assuming a clock frequency in pulse code.
    config.flags = RMT_CHANNEL_FLAGS_AWARE_DFS;
    config.clk_div = 1;
    config.mem_block_num = 1;
    config.tx_config.carrier_en = false;
    config.tx_config.loop_en = false;
    config.tx_config.idle_output_en = true;
    config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
    result = rmt_config(&config);
    if (result != ESP_OK) return fail(result, "rmt_config");
    result = rmt_driver_install(LIGHT_CHANNEL, 0, 0);
    if (result != ESP_OK) return fail(result, "rmt_driver_install");
    driverInstalled = true;
    result = rmt_get_counter_clock(LIGHT_CHANNEL, &counterClockHz);
    if (result != ESP_OK) {
        retireDriver();
        return fail(result, "rmt_counter_clock");
    }
    driverReady = true;
    return ESP_OK;
}

// Caller holds commandMutex. No state is committed until transmission completes.
esp_err_t apply(const State &candidate) {
    haveAttempt = true;
    lastAttemptMs = millis();
    esp_err_t result = ensureDriver();
    if (result != ESP_OK) return result;
    camera_light_model::Pulse pulses[camera_light_model::FRAME_ITEMS];
    if (!camera_light_model::waveform(candidate, counterClockHz, pulses)) {
        retireDriver();
        return fail(ESP_ERR_INVALID_ARG, "rmt_clock_resolution");
    }
    for (size_t i = 0; i < camera_light_model::FRAME_ITEMS; ++i) {
        transmitItems[i].duration0 = pulses[i].duration0;
        transmitItems[i].level0 = pulses[i].level0;
        transmitItems[i].duration1 = pulses[i].duration1;
        transmitItems[i].level1 = pulses[i].level1;
    }
    result = rmt_write_items(LIGHT_CHANNEL, transmitItems,
                             camera_light_model::FRAME_ITEMS, false);
    if (result != ESP_OK) {
        retireDriver();
        return fail(result, "rmt_write");
    }
    // A single transfer is about 330us. This is a driver-fault deadline, not a
    // light-duration limit: the pixel remains latched until another command.
    result = rmt_wait_tx_done(LIGHT_CHANNEL, pdMS_TO_TICKS(100));
    if (result != ESP_OK) {
        retireDriver();
        return fail(result, "rmt_tx_complete");
    }
    state = candidate;
    lastTransmitted = camera_light_model::transmittedRgb(candidate);
    commandKnown = true;
    lastSucceeded = true;
    haveSuccess = true;
    lastSuccessMs = millis();
    lastError = ESP_OK;
    lastErrorStage = "";
    return ESP_OK;
}

void sendError(int code, const char *message) {
    StaticJsonDocument<384> document;
    document["error"] = message;
    String body;
    serializeJson(document, body);
    httpServer->sendHeader("Cache-Control", "no-store");
    httpServer->send(code, "application/json", body);
}

bool authorized() {
    if (!httpServer) return false;
    if (!authorizeRequest) {
        sendError(503, "Light authentication is unavailable");
        return false;
    }
    if (!authorizeRequest()) return false;
    if (!commandMutex) {
        sendError(503, "Light state memory is unavailable");
        return false;
    }
    return true;
}

void putRgb(JsonArray array, const Rgb &rgb) {
    array.add(rgb.red);
    array.add(rgb.green);
    array.add(rgb.blue);
}

void sendState(int code) {
    StaticJsonDocument<1536> document;
    {
        Lock lock;
        document["driver"] = "ws2812-rmt";
        document["gpio"] = static_cast<int>(LIGHT_GPIO);
        document["rmt_channel"] = static_cast<int>(LIGHT_CHANNEL);
        document["pixels"] = 1;
        document["color_order"] = "GRB";
        document["ready"] = driverReady;
        document["hardware_verified"] = false;
        document["optical_readback"] = false;
        document["on"] = state.on;
        document["brightness"] = state.brightness;
        putRgb(document.createNestedArray("rgb"), state.rgb);
        putRgb(document.createNestedArray("last_transmitted_rgb"), lastTransmitted);
        document["command_state_known"] = commandKnown;
        document["last_command_succeeded"] = lastSucceeded;
        if (haveSuccess) document["last_command_ms"] = lastSuccessMs;
        else document["last_command_ms"] = nullptr;
        if (haveAttempt) document["last_attempt_ms"] = lastAttemptMs;
        else document["last_attempt_ms"] = nullptr;
        if (lastError == ESP_OK) document["error"] = nullptr;
        else {
            JsonObject error = document.createNestedObject("error");
            error["code"] = lastError;
            error["name"] = esp_err_to_name(lastError);
            error["stage"] = lastErrorStage;
        }
    }
    String body;
    serializeJson(document, body);
    httpServer->sendHeader("Cache-Control", "no-store");
    httpServer->send(code, "application/json", body);
}

void handleGet() {
    if (!authorized()) return;
    sendState(200);
}

void handlePost() {
    if (!authorized()) return;
    String contentType = httpServer->header("Content-Type");
    const int separator = contentType.indexOf(';');
    if (separator >= 0) contentType.remove(separator);
    contentType.trim();
    if (!contentType.equalsIgnoreCase("application/json")) {
        sendError(415, "Content-Type must be application/json");
        return;
    }
    const String body = httpServer->arg("plain");
    State candidate;
    const char *validationError = nullptr;
    esp_err_t result = ESP_OK;
    bool valid;
    {
        Lock lock;
        valid = camera_light_model::parseUpdate(body.c_str(), body.length(), state,
                                                candidate, validationError);
        if (valid) result = apply(candidate);
    }
    if (!valid) {
        sendError(body.length() > 256 ? 413 : 400, validationError);
        return;
    }
    sendState(result == ESP_OK ? 200 : 503);
}
} // namespace

void begin(WebServer &server, bool (*authorize)()) {
    if (httpServer) return;
    httpServer = &server;
    authorizeRequest = authorize;
    commandMutex = xSemaphoreCreateMutex();
    server.on("/api/light", HTTP_GET, handleGet);
    server.on("/api/light", HTTP_POST, handlePost);
    if (!commandMutex) {
        log_e("Camera light: no memory for command state");
        return;
    }
    esp_err_t result;
    {
        Lock lock;
        result = apply(State()); // explicit off command at boot
    }
    if (result != ESP_OK)
        log_e("Camera light initialization failed: %s (%s)", esp_err_to_name(result), lastErrorStage);
}

bool off() {
    if (!commandMutex) return false;
    Lock lock;
    State candidate = state;
    candidate.on = false;
    return apply(candidate) == ESP_OK;
}
} // namespace camera_light
