/* Host-only actual-pixel oracle. Production renderer is compiled separately as C11. */
#include "PortableDeskClockFaces.h"
#include "../fixtures/desk_clock_reader/DeskClockFaces.h"
#include "../fixtures/desk_clock_reader/UnpaddedLayout.h"
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

struct Canvas {
    int width, height;
    std::vector<unsigned char> pixels;
    unsigned calls = 0, fail_at = 0;
    Canvas(int w, int h) : width(w), height(h), pixels(static_cast<size_t>(w * h), 0) {}
    int getScreenWidth() const { return width; }
    int getScreenHeight() const { return height; }
    void fillRect(int x, int y, int w, int h, bool black = true) {
        assert(x >= 0 && y >= 0 && w > 0 && h > 0 && x <= width - w && y <= height - h);
        for (int row = y; row < y + h; ++row)
            std::memset(&pixels[static_cast<size_t>(row * width + x)], black ? 1 : 0, static_cast<size_t>(w));
    }
    static bool fill(void *context, int x, int y, int w, int h, bool black) {
        Canvas &c = *static_cast<Canvas *>(context);
        ++c.calls;
        assert(c.calls <= 65536u);
        if (c.fail_at && c.calls == c.fail_at) return false;
        assert(!c.fail_at || c.calls < c.fail_at);  // no delivery after failure
        c.fillRect(x, y, w, h, black);
        return true;
    }
    bool draw(unsigned face, int hour, int minute, bool format, bool valid = true) {
        calls = 0;
        portable_desk_canvas c = {this, width, height, fill};
        return portable_desk_draw_face(&c, face, hour, minute, format, valid);
    }
    void write() const {
        std::printf("P4\n%d %d\n", width, height);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; x += 8) {
                unsigned byte = 0;
                for (int b = 0; b < 8 && x + b < width; ++b)
                    byte |= pixels[static_cast<size_t>(y * width + x + b)] << (7 - b);
                assert(std::putchar(static_cast<int>(byte)) != EOF);
            }
        }
    }
};

static void invalid_inputs() {
    Canvas c(480, 800);
    portable_desk_canvas good = {&c, 480, 800, Canvas::fill};
    assert(!portable_desk_draw_face(nullptr, 0, 0, 0, false, true));
    for (int dimension : {INT_MIN, -1, 0, 1, 239, 2049, INT_MAX}) {
        portable_desk_canvas bad = good;
        bad.width = dimension;
        assert(!portable_desk_draw_face(&bad, 0, 0, 0, false, true));
        bad = good; bad.height = dimension;
        assert(!portable_desk_draw_face(&bad, 0, 0, 0, false, true));
    }
    portable_desk_canvas bad = good; bad.fill_rect = nullptr;
    assert(!portable_desk_draw_face(&bad, 0, 0, 0, false, true));
    assert(c.calls == 0);
    assert(c.draw(0, 10, 8, false));
    const auto segment = c.pixels;
    for (unsigned face : {6u, 255u, 256u, UINT_MAX}) {
        assert(c.draw(face, 10, 8, false)); assert(c.pixels == segment);
    }
    assert(c.draw(0, 0, 0, false, false));
    const auto missing = c.pixels;
    for (unsigned face = 0; face < 6; ++face) for (bool format : {false, true}) {
        for (int hour : {INT_MIN, -1, 24, INT_MAX}) {
            assert(c.draw(face, hour, 30, format)); assert(c.pixels == missing);
        }
        for (int minute : {INT_MIN, -1, 60, INT_MAX}) {
            assert(c.draw(face, 12, minute, format)); assert(c.pixels == missing);
        }
        assert(c.draw(face, INT_MIN, INT_MAX, format, false)); assert(c.pixels == missing);
        for (unsigned fail : {1u, 2u, 10u}) {
            c.fail_at = fail;
            assert(!c.draw(face, 12, 30, format)); assert(c.calls == fail);
        }
        c.fail_at = 0;
    }
}

static void boundaries() {
    for (const auto &size : {std::pair<int,int>{240,240}, {241,243}, {2048,240}, {240,2048}, {2048,2048}}) {
        Canvas c(size.first, size.second);
        for (unsigned face = 0; face < 6; ++face) for (bool format : {false, true}) {
            assert(c.draw(face, 23, 59, format)); const auto previous = c.pixels;
            assert(c.draw(face, 0, 0, format));
            assert(c.draw(face, 23, 59, format)); assert(c.pixels == previous);
        }
    }
    // Every hour/minute and format: bounded callback delivery, no invalid indexing.
    Canvas c(240, 240);
    for (unsigned face = 0; face < 6; ++face) for (int hour = 0; hour < 24; ++hour)
        for (int minute = 0; minute < 60; ++minute) for (bool format : {false, true})
            assert(c.draw(face, hour, minute, format));
}

static void policy_reconstruction() {
    static_assert(PORTABLE_DESK_CLOCK_RECORD_BYTES == 80, "Retained schema changed");
    static_assert(PORTABLE_DESK_SEGMENTS == 0 && PORTABLE_DESK_SANS == 1 && PORTABLE_DESK_SERIF == 2 &&
        PORTABLE_DESK_MINIMAL == 3 && PORTABLE_DESK_RAILWAY == 4 && PORTABLE_DESK_DECO == 5, "Face IDs changed");
    Canvas c(480, 800);
    for (unsigned face = 0; face < 6; ++face) for (unsigned format = 0; format < 2; ++format) {
        portable_desk_record r = {};
        r.config.face = static_cast<uint8_t>(face); r.config.time_format = static_cast<uint8_t>(format);
        std::strcpy(r.config.time_zone, "UTC0"); r.displayed_minute = 23 * 3600 + 59 * 60;
        r.has_image = true; r.refresh_modulo = 1;
        uint8_t bytes[80]; assert(portable_desk_encode(&r, bytes, sizeof(bytes)));
        assert(c.draw(face, 23, 59, format == 0)); auto previous = c.pixels;
        assert(c.draw(face, 0, 0, format == 0));
        portable_desk_record restored = {}; assert(portable_desk_decode(bytes, sizeof(bytes), &restored));
        portable_desk_cycle cycle = {};
        assert(portable_desk_begin(&cycle, &restored.config, &restored, true));
        portable_desk_frame frame = {}; assert(portable_desk_plan_frame(&cycle, 24 * 3600, &frame));
        assert(frame.has_previous && frame.previous_minute == r.displayed_minute);
        int old_hour = static_cast<int>((frame.previous_minute / 3600) % 24);
        int old_minute = static_cast<int>((frame.previous_minute / 60) % 60);
        assert(c.draw(restored.config.face, old_hour, old_minute, restored.config.time_format == 0)); assert(c.pixels == previous);
    }
}

int main(int argc, char **argv) {
    if (argc == 10 && !std::strcmp(argv[1], "render")) {
        unsigned face = static_cast<unsigned>(std::strtoul(argv[2], nullptr, 10));
        Canvas c(std::atoi(argv[3]), std::atoi(argv[4]));
        int hour = std::atoi(argv[5]), minute = std::atoi(argv[6]);
        bool format = std::atoi(argv[7]) != 0, valid = std::atoi(argv[8]) != 0;
        if (std::atoi(argv[9]) == 2) DeskClockUnpaddedLayout::draw(c, static_cast<uint8_t>(face), hour, minute, format, valid);
        else if (std::atoi(argv[9]) == 1) DeskClockFaces::draw(c, static_cast<uint8_t>(face), hour, minute, format, valid);
        else assert(c.draw(face, hour, minute, format, valid));
        c.write(); return 0;
    }
    invalid_inputs(); boundaries(); policy_reconstruction();
    std::puts("Desk faces: 17,280 full-day face/format rasters; bounds/failure/invalid cases; 80-byte retained old-frame reconstruction pass");
}
