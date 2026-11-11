/* Paper Space's first external Home entry point. Rendering, selection and
 * input state belong to this ELF. No Reader/firmware GUI symbols are imported.
 * Home ordering, wrapped navigation and typography derive from Reader;
 * provenance and remaining feature migration are recorded in SOURCES.json. */
#include "PaperSpace.h"
#include "sdk/RiscRuntimeV1.h"
#include "sdk/RiscDisplayOutputV1.h"
#include "sdk/RiscInputNavigationV1.h"
#include "fonts/PaperSpaceText12.h"
#include <string.h>
#ifdef PAPER_SPACE_TOUCH
#include "sdk/PortableTouch.h"
#endif
#define EXPORT __attribute__((visibility("default")))
#define PRESENT_DEADLINE_MS 20000u
#define MAX_SURFACE_BYTES (2u * 1024u * 1024u)
#define NAV_BITS 511u
static const risc_runtime_api_v1 *runtime;
static risc_runtime_capability_v1 display_grant, navigation_grant;
static const risc_display_output_api_v1 *display;
static const risc_input_navigation_api_v1 *navigation;
static risc_display_info_v1 info;
static risc_display_surface_v1 surface;
static unsigned selected, page_start, visible_rows;
static int left, right, top, bottom, menu_top, row_height;
static uint32_t format, previous_buttons;
static bool ready, neutral, confirm_armed, foreground_claimed;
#ifdef PAPER_SPACE_TOUCH
static portable_touch touch;
#endif
static void diagnostic(const char *message) {
    if (runtime && runtime->diagnostic) runtime->diagnostic(message);
}
static bool now(uint32_t *value) {
    risc_runtime_health_v1 health = {0}; health.struct_size = sizeof(health);
    if (!runtime->health(&health)) return false;
    *value = health.uptime_ms; return true;
}
static bool path_valid(const char *path) {
    size_t length = 0, component = 0;
    if (!path || !*path || path[0] == '/') return false;
    for (; length < PAPER_SPACE_PATH_BYTES && path[length]; ++length) {
        unsigned char c = (unsigned char)path[length];
        if (c == '/') {
            if (!component) return false;
            component = 0;
        } else {
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return false;
            if (c == '.' && (component == 0 || (length && path[length-1] == '.'))) return false;
            ++component;
        }
    }
    return length > 4 && length < PAPER_SPACE_PATH_BYTES && component > 4 &&
           !strcmp(path + length - 4, ".elf") && strcmp(path, "default.elf");
}
static bool catalog_valid(void) {
    if (paper_space_item_count > PAPER_SPACE_MAX_ITEMS) return false;
    for (unsigned i = 0; i < paper_space_item_count; ++i) {
        size_t length = 0;
        for (; length < PAPER_SPACE_LABEL_BYTES && paper_space_items[i].label[length]; ++length) {
            unsigned char c = (unsigned char)paper_space_items[i].label[length];
            if (c < 32 || c > 126) return false;
        }
        if (!length || length == PAPER_SPACE_LABEL_BYTES || !path_valid(paper_space_items[i].path)) return false;
        for (unsigned j = 0; j < i; ++j)
            if (!strcmp(paper_space_items[i].path, paper_space_items[j].path)) return false;
    }
    return true;
}
static bool cleanup(void) {
    bool clean = true;
    ready = false;
    if (surface.frame) {
        display->release(display->context, surface.frame);
        memset(&surface, 0, sizeof(surface));
    }
    if (foreground_claimed) {
        if (!navigation->foreground(navigation->context, NULL, 0)) clean = false;
        else foreground_claimed = false;
    }
#ifdef PAPER_SPACE_TOUCH
    if (!portable_touch_close(&touch, runtime)) clean = false;
#endif
    /* A failed foreground rollback is still an owned provider operation.
     * Keep its grant pinned so fini may retry without a revoked interface. */
    if (!foreground_claimed && navigation_grant.api && !runtime->release(&navigation_grant)) clean = false;
    if (display_grant.api && !runtime->release(&display_grant)) clean = false;
    if (!clean) diagnostic("PAPER_SPACE cleanup=failed restart-required");
    return clean;
}
static void pixel(int x, int y, bool black) {
    if (x < left || x >= right || y < top || y >= bottom) return;
    unsigned char *row = (unsigned char *)surface.pixels + (size_t)y * surface.stride_bytes;
    if (format == RISC_DISPLAY_FORMAT_MONO1) {
        const unsigned char bit = (unsigned char)(0x80u >> ((unsigned)x & 7u));
        if (black) row[(unsigned)x >> 3] |= bit;
        else row[(unsigned)x >> 3] &= (unsigned char)~bit;
    } else {
        unsigned char value = black ? 0 : 255;
        row[(size_t)x * 2] = value; row[(size_t)x * 2 + 1] = value;
    }
}
static void rectangle(int x, int y, int width, int height, bool black) {
    int x1 = x < left ? left : x, y1 = y < top ? top : y;
    int x2 = x + width > right ? right : x + width;
    int y2 = y + height > bottom ? bottom : y + height;
    for (int yy = y1; yy < y2; ++yy)
        for (int xx = x1; xx < x2; ++xx) pixel(xx, yy, black);
}
static void text(int x, int baseline, const char *value, bool black) {
    /* Original Reader glyph metrics use 12.4 fixed point. This bounded ASCII
     * subset keeps pixels/advances; full shaping, kerning and SD fonts are later. */
    int position = x * 16;
    for (size_t n = 0; n < 96 && value[n]; ++n) {
        unsigned char c = (unsigned char)value[n];
        if (c < 32 || c > 126) c = '?';
        const paper_glyph *g = &paper_text_glyphs[c - 32];
        int gx = (position + 8) / 16 + g->left, gy = baseline - g->top;
        if (gx + g->width >= right - 8) break;
        for (unsigned row = 0; row < g->height; ++row)
            for (unsigned col = 0; col < g->width; ++col) {
                unsigned bit = row * g->width + col;
                if (paper_text_pixels[g->offset + bit / 8] & (0x80u >> (bit & 7u)))
                    pixel(gx + (int)col, gy + (int)row, black);
            }
        position += g->advance;
    }
}
static int text_width(const char *value) {
    unsigned advance = 0;
    for (size_t n = 0; n < 48 && value[n]; ++n) {
        unsigned char c = (unsigned char)value[n];
        if (c < 32 || c > 126) c = '?';
        advance += paper_text_glyphs[c - 32].advance;
    }
    return (int)((advance + 8u) / 16u);
}
static bool draw_home(void) {
    memset(&surface, 0, sizeof(surface));
    if (!display->acquire(display->context, format, &surface)) return false;
    const uint32_t minimum_stride = format == RISC_DISPLAY_FORMAT_MONO1 ?
                                   (info.width + 7u) / 8u : info.width * 2u;
    const uint64_t bytes = (uint64_t)surface.stride_bytes * surface.height;
    if (!surface.frame || !surface.pixels || surface.width != info.width ||
        surface.height != info.height || surface.pixel_format != format ||
        surface.stride_bytes < minimum_stride || bytes > surface.size_bytes ||
        surface.size_bytes > MAX_SURFACE_BYTES) return false;
    memset(surface.pixels, format == RISC_DISPLAY_FORMAT_MONO1 ? 0 : 255, surface.size_bytes);
    text(left + 16, top + 38, "Paper Space", true);
    rectangle(left + 16, top + 54, right - left - 32, 1, true);
    if (bottom - top >= 400) {
        text(left + 16, top + 98, "Home", true);
        text(left + 16, top + 132, "Apps and Settings", true);
    }
    page_start = visible_rows ? (selected / visible_rows) * visible_rows : 0;
    if (!paper_space_item_count) text(left + 16, menu_top + 31, "No apps configured", true);
    for (unsigned row = 0; row < visible_rows && page_start + row < paper_space_item_count; ++row) {
        unsigned index = page_start + row;
        int y = menu_top + (int)row * row_height;
        bool chosen = index == selected;
        rectangle(left + 12, y, right - left - 24, row_height - 4, chosen);
        text(left + 24, y + 30, paper_space_items[index].label, !chosen);
        runtime->yield_ms(1);
    }
    rectangle(left + 16, bottom - 52, right - left - 32, 1, true);
    const int hint_width = (right - left) / 3;
    const char *hints[] = {"Select", "Up", "Down"};
    for (unsigned i = 0; i < 3; ++i)
        text(left + (int)i * hint_width + (hint_width - text_width(hints[i])) / 2,
             bottom - 20, hints[i], true);
    risc_display_present_options_v1 options = {RISC_DISPLAY_PRESENT_QUALITY, RISC_DISPLAY_QUEUE_FIFO, 0};
    risc_display_present_token_v1 token = 0;
    if (!display->submit(display->context, surface.frame, NULL, 0, &options, &token)) return false;
    /* True transfers ownership even when a broken provider returns token 0.
     * Never release or write a transferred frame while rejecting that token. */
    memset(&surface, 0, sizeof(surface));
    if (!token) return false;
    uint32_t start;
    if (!now(&start)) return false;
    for (unsigned step = 0; step < PRESENT_DEADLINE_MS; ++step) {
        risc_display_present_status_v1 status = {0}; uint32_t current;
        if (!display->present_status(display->context, token, &status)) return false;
        if (status.state == RISC_DISPLAY_PRESENT_COMPLETE) return true;
        if (status.state != RISC_DISPLAY_PRESENT_ACTIVE && status.state != RISC_DISPLAY_PRESENT_QUEUED) return false;
        if (!now(&current) || (uint32_t)(current - start) >= PRESENT_DEADLINE_MS) return false;
        runtime->yield_ms(1);
    }
    return false;
}
static bool initialize(void) {
    runtime = risc_runtime_get_api(1);
    if (!runtime || runtime->api_version != 1 || runtime->struct_size < RISC_RUNTIME_CAPABILITIES_V1_SIZE ||
        !runtime->health || !runtime->yield_ms || !runtime->diagnostic || !runtime->request_launch ||
        !runtime->acquire || !runtime->release) { runtime = NULL; return false; }
    if (!catalog_valid()) return false;
    selected = previous_buttons = 0; neutral = confirm_armed = foreground_claimed = false;
    display_grant.struct_size = sizeof(display_grant);
    if (!runtime->acquire("display.output", 1, 0, &display_grant)) return false;
    display = display_grant.api;
    if (!display || display->api_version != 1 || display->struct_size < sizeof(*display) ||
        !display->get_info || !display->acquire || !display->release || !display->submit || !display->present_status) return false;
    memset(&info, 0, sizeof(info)); info.struct_size = sizeof(info);
    if (!display->get_info(display->context, &info) || info.api_version != 1 || info.struct_size < sizeof(info) ||
        info.width < 240 || info.width > 1024 || info.height < 240 || info.height > 1024) return false;
    format = (info.supported_formats & RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_MONO1)) ?
             RISC_DISPLAY_FORMAT_MONO1 : RISC_DISPLAY_FORMAT_RGB565;
    if (!(info.supported_formats & RISC_DISPLAY_FORMAT_BIT(format))) return false;
    left = info.safe_insets.left; right = (int)info.width - info.safe_insets.right;
    top = info.safe_insets.top; bottom = (int)info.height - info.safe_insets.bottom;
    if (right - left < 240 || bottom - top < 240) return false;
    menu_top = top + (bottom - top >= 400 ? 170 : 76); row_height = 48;
    visible_rows = (unsigned)(bottom - 64 - menu_top) / (unsigned)row_height;
    if (!visible_rows) return false;
#ifdef PAPER_SPACE_NAVIGATION
    navigation_grant.struct_size = sizeof(navigation_grant);
    if (!runtime->acquire("input.navigation", 1, 0, &navigation_grant)) return false;
    navigation = navigation_grant.api;
    if (!navigation || navigation->api_version != 1 || navigation->struct_size < sizeof(*navigation) ||
        !navigation->poll || !navigation->reset || !navigation->foreground || !navigation->reset(navigation->context)) return false;
#endif
#ifdef PAPER_SPACE_TOUCH
    if (!portable_touch_open(&touch, runtime)) return false;
    risc_touch_snapshot_v1 initial_touch = {0};
    if (!touch.api->snapshot(touch.api->context, &initial_touch) ||
        initial_touch.width != info.width || initial_touch.height != info.height) return false;
#ifdef PAPER_SPACE_NAVIGATION
    const risc_input_foreground_v1 claims[] = {{"input.touch.raw", 1}};
    /* The contract stays suppressed even on failure: every attempted claim
     * must be rolled back, including partially successful initialization. */
    foreground_claimed = true;
    if (!navigation->foreground(navigation->context, claims, 1)) return false;
#endif
#endif
    ready = true; return true;
}
EXPORT int app_module_init(void) {
    if (!initialize()) {
        diagnostic("PAPER_SPACE init=failed");
        if (runtime && runtime->release) cleanup();
        return -1;
    }
    return 0;
}
EXPORT void app_module_fini(void) { if (runtime && runtime->release) cleanup(); }
EXPORT void app_main(void) {
    if (!ready) return;
    diagnostic("PAPER_SPACE ready version=1.0.0 selection=0");
    bool dirty = true;
    while (ready) {
        uint32_t tick;
        if (!now(&tick)) break;
        if (dirty) {
            if (!draw_home()) { diagnostic("PAPER_SPACE display=failed"); break; }
            dirty = false;
        }
        bool activate = false;
#ifdef PAPER_SPACE_NAVIGATION
        risc_input_navigation_frame_v1 input = {0};
        if (!navigation->poll(navigation->context, &input) ||
            ((input.buttons | input.pressed | input.released) & ~NAV_BITS)) {
            neutral = confirm_armed = false; previous_buttons = 0;
            runtime->yield_ms(10); continue;
        }
        if (!neutral) {
            if (!input.buttons && !input.pressed) neutral = true;
            previous_buttons = input.buttons;
        } else {
            const uint32_t down = input.pressed & ~previous_buttons;
            const uint32_t up = input.released;
            if (down & RISC_NAV_CONFIRM) confirm_armed = true;
            const uint32_t next = down & (RISC_NAV_DOWN | RISC_NAV_RIGHT | RISC_NAV_PAGE_FORWARD);
            const uint32_t prior = down & (RISC_NAV_UP | RISC_NAV_LEFT | RISC_NAV_PAGE_BACK);
            if ((next != 0) != (prior != 0)) {
                if (paper_space_item_count) selected = next ? (selected + 1u) % paper_space_item_count :
                   (selected + paper_space_item_count - 1u) % paper_space_item_count;
                dirty = true; confirm_armed = false;
            }
            if (down & (RISC_NAV_HOME | RISC_NAV_BACK)) { selected = 0; dirty = true; confirm_armed = false; }
            if ((up & RISC_NAV_CONFIRM) && confirm_armed && !input.buttons) activate = true;
            if (up & RISC_NAV_CONFIRM) confirm_armed = false;
            previous_buttons = input.buttons;
        }
#endif
#ifdef PAPER_SPACE_TOUCH
        uint16_t tx, ty;
        if (portable_touch_tap(&touch, &tx, &ty) && tx >= left && tx < right &&
            paper_space_item_count) {
            if (ty >= bottom - 52 && ty < bottom) {
                /* These three visible controls also make every page reachable
                 * in touch-only builds, including the smallest viewport. */
                unsigned action = (unsigned)(tx - left) * 3u / (unsigned)(right - left);
                if (!action) activate = true;
                else {
                    selected = action == 1 ? (selected + paper_space_item_count - 1u) % paper_space_item_count :
                                             (selected + 1u) % paper_space_item_count;
                    dirty = true;
                }
            } else if (tx >= left + 12 && tx < right - 12 && ty >= menu_top && ty < bottom - 64) {
                unsigned row = (unsigned)(ty - menu_top) / (unsigned)row_height;
                if (row < visible_rows && page_start + row < paper_space_item_count) {
                    selected = page_start + row; activate = true;
                }
            }
        }
#endif
        if (activate && selected < paper_space_item_count) {
            char path[PAPER_SPACE_PATH_BYTES];
            memcpy(path, paper_space_items[selected].path, sizeof(path));
            /* Close app-local operations before queueing, then return at once.
             * Runtime finalizes/unmaps us before any child is loaded. */
            if (!cleanup()) break;
            if (runtime->request_launch(path)) return;
            diagnostic("PAPER_SPACE launch=denied");
            if (!initialize()) break;
            dirty = true;
        }
        runtime->yield_ms(10);
    }
}
