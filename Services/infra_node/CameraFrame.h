#pragma once
#include <RiscStillCameraV1.h>

// Product extension of the published v1 prefix. Existing consumers can still
// request the small identification JPEG through legacy.jpeg.
struct InfraCameraFrame {
    uint8_t* bytes;
    uint32_t length;
    uint16_t width, height;
};
struct InfraCameraApi {
    risc_still_camera_v1 legacy;
    // (0,0) selects the detected sensor maximum. Release successful results.
    bool (*capture)(uint16_t width, uint16_t height, InfraCameraFrame* out);
    bool (*upload)(const char* url, const char* bearer, const char* siteBearer, const InfraCameraFrame* frame);
    void (*release)(InfraCameraFrame* frame);
};
