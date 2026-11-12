#include "ultrasonic_telemetry.h"
#include "ultrasonic_model.h"
#include "ultrasonic_capture_model.h"
#include <Arduino.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "Four-sonar profile requires ESP32-S3 CAM N16R8"
#endif

namespace ultrasonic_telemetry {
namespace {
constexpr uint8_t TRIG[4] = {1, 2, 14, 21};
constexpr uint8_t ECHO[4] = {40, 3, 45, 46};
const char *const IDS[4] = {"front", "left", "right", "rear"};
// ECHO GPIO3/45/46 are strapping pins. Read ULTRASONIC.md before wiring.
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
using ultrasonic_capture::Sample;
Sample samples[4];
// Eight seconds of per-transducer history at the 260ms revisit interval.
// Camera capture selects causally, even if its HTTP request is served later.
constexpr size_t HISTORY = 32;
Sample history[4][HISTORY];
size_t historyNext[4] = {};
bool ready = false, isPaused = false;
volatile int active = -1;
volatile bool risen = false, done = false;
volatile uint32_t riseUs = 0, widthUs = 0;
TaskHandle_t workerHandle = nullptr;

void IRAM_ATTR echoEdge(void *arg) {
    const int index = static_cast<int>(reinterpret_cast<uintptr_t>(arg));
    const uint32_t now = micros();
    const bool high = gpio_get_level(static_cast<gpio_num_t>(ECHO[index]));
    portENTER_CRITICAL_ISR(&mux);
    if (active == index && !done) {
        if (high && !risen) { riseUs = now; risen = true; }
        else if (!high && risen) { widthUs = now - riseUs; done = true; }
    }
    portEXIT_CRITICAL_ISR(&mux);
}

void worker(void *) {
    uint8_t index = 0;
    for (;;) {
        portENTER_CRITICAL(&mux);
        const bool paused = isPaused;
        portEXIT_CRITICAL(&mux);
        if (paused) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }
        const uint32_t started = millis();
        const bool stuck = gpio_get_level(static_cast<gpio_num_t>(ECHO[index]));
        portENTER_CRITICAL(&mux);
        risen = done = false;
        widthUs = 0;
        active = stuck ? -1 : index;
        portEXIT_CRITICAL(&mux);
        bool complete = false, aborted = false;
        uint32_t pulse = 0;
        uint64_t triggeredUs = esp_timer_get_time();
        if (!stuck) {
            digitalWrite(TRIG[index], HIGH);
            triggeredUs = esp_timer_get_time();
            delayMicroseconds(12);
            digitalWrite(TRIG[index], LOW);
            const uint32_t triggered = micros();
            while (uint32_t(micros() - triggered) < ultrasonic_model::TIMEOUT_US) {
                portENTER_CRITICAL(&mux);
                complete = done;
                aborted = isPaused;
                pulse = widthUs;
                portEXIT_CRITICAL(&mux);
                if (complete || aborted) break;
                vTaskDelay(1);
            }
        }
        portENTER_CRITICAL(&mux);
        // Read the final ISR result atomically with disarming.
        complete = done;
        pulse = widthUs;
        active = -1;
        Sample &s = samples[index];
        s.sampled = true;
        s.completedUs = esp_timer_get_time();
        s.observedUs = complete ? triggeredUs + pulse / 2 : s.completedUs;
        s.at = static_cast<uint32_t>(s.observedUs / 1000);
        ++s.sequence;
        s.pulse = pulse;
        s.valid = complete && ultrasonic_model::inRange(pulse) && !isPaused && !aborted;
        s.status = isPaused || aborted ? "paused" : stuck ? "echo_stuck_high" :
            !complete ? "timeout" : !ultrasonic_model::inRange(pulse) ? "out_of_range" : "ok";
        history[index][historyNext[index]] = s;
        historyNext[index] = (historyNext[index] + 1) % HISTORY;
        portEXIT_CRITICAL(&mux);
        // One transmitter at a time, >=65ms between all sensors' bursts.
        while (uint32_t(millis() - started) < ultrasonic_model::TRIGGER_INTERVAL_MS)
            vTaskDelay(1);
        index = (index + 1) % 4;
    }
}
}
void begin() {
    if (workerHandle) return;
    for (int i = 0; i < 4; ++i) {
        digitalWrite(TRIG[i], LOW);
        pinMode(TRIG[i], OUTPUT);
        pinMode(ECHO[i], INPUT_PULLDOWN);
        attachInterruptArg(ECHO[i], echoEdge, reinterpret_cast<void *>(static_cast<uintptr_t>(i)), CHANGE);
    }
    const bool created = xTaskCreate(worker, "sonar", 3072, nullptr, 1, &workerHandle) == pdPASS;
    portENTER_CRITICAL(&mux);
    ready = created;
    portEXIT_CRITICAL(&mux);
}
void pause(bool paused) {
    portENTER_CRITICAL(&mux);
    isPaused = paused;
    if (paused) {
        active = -1;
        // Resume cannot resurrect readings captured before an OTA pause.
        for (auto &s : samples) s.valid = false;
        for (auto &sensor : history) for (auto &s : sensor) s = Sample{};
    }
    portEXIT_CRITICAL(&mux);
}
void writeJsonAt(JsonObject output, uint64_t captureUs, bool compact) {
    Sample copy[4];
    bool paused, running;
    portENTER_CRITICAL(&mux);
    for (int i = 0; i < 4; ++i) {
        copy[i] = ultrasonic_capture::selectAt(history[i], captureUs);
    }
    paused = isPaused; running = ready;
    portEXIT_CRITICAL(&mux);
    const uint32_t now = static_cast<uint32_t>(captureUs / 1000);
    output["schema"] = "hcsr04.v1";
    output["sample_clock"] = "esp_timer";
    output["capture_monotonic_us"] = captureUs;
    output["worker_ready"] = running;
    output["paused"] = paused;
    output["trigger_interval_ms"] = ultrasonic_model::TRIGGER_INTERVAL_MS;
    output["stale_after_ms"] = ultrasonic_model::STALE_MS;
    output["sound_speed_m_s"] = 340;
    JsonArray sensors = output.createNestedArray("sensors");
    for (int i = 0; i < 4; ++i) {
        const Sample &s = copy[i];
        const bool valid = running && ultrasonic_model::fresh(s.sampled, s.valid, paused, now, s.at);
        JsonObject item = sensors.createNestedObject();
        item["id"] = IDS[i];
        if (!compact) {
            item["trig_gpio"] = TRIG[i];
            item["echo_gpio"] = ECHO[i];
        }
        item["sequence"] = s.sequence;
        item["valid"] = valid;
        item["status"] = !running ? "worker_unavailable" : paused ? "paused" :
            s.valid && !valid ? "stale" : s.status;
        if (s.sampled) {
            item["observed_monotonic_us"] = s.observedUs;
            item["completed_monotonic_us"] = s.completedUs;
            item["observed_uptime_ms"] = s.at;
            item["age_ms"] = uint32_t(now - s.at);
        } else {
            item["observed_uptime_ms"] = nullptr; item["age_ms"] = nullptr;
        }
        if (valid) {
            item["distance_m"] = ultrasonic_model::metres(s.pulse);
            item["echo_us"] = s.pulse;
        } else {
            item["distance_m"] = nullptr; item["echo_us"] = nullptr;
        }
    }
}
void writeJson(JsonObject output, bool compact) {
    writeJsonAt(output, esp_timer_get_time(), compact);
}
}
