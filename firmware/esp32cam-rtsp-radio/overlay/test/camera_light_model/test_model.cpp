#include "camera_light_model.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace camera_light_model;

static State update(const char *json, const State &before) {
    State next = before;
    const char *error = nullptr;
    assert(parseUpdate(json, std::strlen(json), before, next, error));
    assert(error == nullptr);
    return next;
}

static void defaults_and_off_memory() {
    State state;
    assert(!state.on && state.brightness == 128);
    assert(state.rgb.red == 255 && state.rgb.green == 255 && state.rgb.blue == 255);
    state = update("{\"brightness\":64,\"rgb\":[12,34,56]}", state);
    assert(!state.on && state.brightness == 64 && state.rgb.green == 34);
    Rgb rgb = transmittedRgb(state);
    assert(rgb.red == 0 && rgb.green == 0 && rgb.blue == 0);
    state = update("{\"on\":true}", state);
    assert(state.on && state.rgb.blue == 56 && state.brightness == 64);
    state = update("{\"on\":false}", state);
    assert(!state.on && state.rgb.red == 12 && state.brightness == 64);
}

static void integer_boundaries_and_scaling() {
    State state = update("{\"on\":true,\"brightness\":255,\"rgb\":[0,128,255]}", State());
    Rgb rgb = transmittedRgb(state);
    assert(rgb.red == 0 && rgb.green == 128 && rgb.blue == 255);
    state = update("{\"brightness\":128}", state);
    rgb = transmittedRgb(state);
    assert(rgb.red == 0 && rgb.green == 64 && rgb.blue == 128);
    state = update("{\"brightness\":0}", state);
    rgb = transmittedRgb(state);
    assert(state.on && rgb.red == 0 && rgb.green == 0 && rgb.blue == 0);
    assert(scale(1, 128) == 1 && scale(255, 1) == 1);
}

static void invalid_updates_are_atomic() {
    const char *invalid[] = {
        "{}", "[]", "null", "true", "{\"on\":1}", "{\"on\":\"true\"}",
        "{\"brightness\":-1}", "{\"brightness\":256}", "{\"brightness\":1.5}",
        "{\"brightness\":true}", "{\"rgb\":[0,1]}", "{\"rgb\":[0,1,2,3]}",
        "{\"rgb\":[-1,0,0]}", "{\"rgb\":[0,256,0]}", "{\"rgb\":[0,0,false]}",
        "{\"rgb\":[0,0,1.0]}", "{\"on\":true,\"pin\":4}",
        "{\"on\":true,\"duration_ms\":1000}", "{\"rgb\":\"white\"}",
        "{\"on\":true,\"brightness\":-1}", "{\"on\":true} trailing",
        "{\"on\":true}{\"on\":false}", "{\"on\":true}x", "{\"on\":true}}",
        "{\"on\\u0000ignored\":true}",
        "{\"on\":tru}", "{\"brightness\":null}", "{\"on\":false,\"unknown\":1}"
    };
    State before = update("{\"on\":true,\"brightness\":17,\"rgb\":[1,2,3]}", State());
    for (const char *json : invalid) {
        State after = before;
        const char *error = nullptr;
        const bool accepted = parseUpdate(json, std::strlen(json), before, after, error);
        if (accepted) std::cerr << "Unexpectedly accepted: " << json << "\n";
        assert(!accepted);
        assert(error != nullptr);
        assert(same(before, after));
    }
}

static void whitespace_and_payload_bounds() {
    State state = update(" \n {\"on\":true} \t\r\n", State());
    assert(state.on);
    char oversized[258];
    std::memset(oversized, ' ', sizeof(oversized));
    State after = state;
    const char *error = nullptr;
    assert(!parseUpdate(oversized, sizeof(oversized), state, after, error));
    assert(same(after, state));
    assert(!parseUpdate(nullptr, 0, state, after, error));
}

static void grb_wire_order_and_latch() {
    State state = update("{\"on\":true,\"brightness\":255,\"rgb\":[128,1,2]}", State());
    Pulse pulses[FRAME_ITEMS];
    assert(waveform(state, 40000000, pulses));
    assert(pulses[0].duration0 == 14); // G bit 7 is zero
    assert(pulses[7].duration0 == 34); // G bit 0 is one
    assert(pulses[8].duration0 == 34); // R bit 7 is one
    assert(pulses[15].duration0 == 14);
    assert(pulses[22].duration0 == 34); // B bit 1 is one
    assert(pulses[23].duration0 == 14);
    uint32_t duration = 0;
    for (size_t i = 0; i < 24; ++i) {
        assert(pulses[i].level0 && !pulses[i].level1);
        assert(pulses[i].duration0 + pulses[i].duration1 == 50);
        duration += pulses[i].duration0 + pulses[i].duration1;
    }
    assert(!pulses[24].level0 && !pulses[24].level1);
    assert(pulses[24].duration0 == 6000 && pulses[24].duration1 == 6000);
    duration += pulses[24].duration0 + pulses[24].duration1;
    assert(duration == 13200); // 330us, then pixel stays latched
}

static void off_and_clock_resolution() {
    Pulse pulses[FRAME_ITEMS];
    assert(waveform(State(), 40000000, pulses));
    for (size_t i = 0; i < 24; ++i)
        assert(pulses[i].duration0 == 14 && pulses[i].duration1 == 36);
    assert(!waveform(State(), 0, pulses));
    assert(!waveform(State(), 1000000, pulses));
    assert(!waveform(State(), 1000000000, pulses));
}

int main() {
    defaults_and_off_memory();
    integer_boundaries_and_scaling();
    invalid_updates_are_atomic();
    whitespace_and_payload_bounds();
    grb_wire_order_and_latch();
    off_and_clock_resolution();
    std::cout << "6 camera light model tests passed; invalid-request cases: 28\n";
}
