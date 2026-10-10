/* Portable C adaptation of Reader DeskClockFaces.h at 34d8e694.
 * MIT source license: repository LICENSE. OFL numeral notices and exact input
 * hashes: ../desk_clock/. No runtime/driver imports or heap allocation. */
#include "PortableDeskClockFaces.h"
#include "../desk_clock/digits.inc"
#include "../desk_clock/sine.inc"

typedef struct { const portable_desk_canvas *canvas; bool ok; } Painter;
typedef struct { int x, y; } Point;
static int minimum(int a, int b) { return a < b ? a : b; }
static int maximum(int a, int b) { return a > b ? a : b; }
static int absolute(int a) { return a < 0 ? -a : a; }

static void rect(Painter *p, int x, int y, int w, int h, bool black) {
    if (!p->ok || w <= 0 || h <= 0) return;
    int right = minimum(x + w, p->canvas->width);
    int bottom = minimum(y + h, p->canvas->height);
    x = maximum(x, 0); y = maximum(y, 0);
    if (right > x && bottom > y)
        p->ok = p->canvas->fill_rect(p->canvas->context, x, y, right - x, bottom - y, black);
}

/* Restoring integer square root: at most 16 iterations, exact floor. */
static int root(uint32_t n) {
    uint32_t result = 0, bit = UINT32_C(1) << 30;
    while (bit > n) bit >>= 2;
    while (bit) {
        if (n >= result + bit) { n -= result + bit; result = (result >> 1) + bit; }
        else result >>= 1;
        bit >>= 2;
    }
    return (int)result;
}

/* One step is half a degree (one minute of hour-hand motion). Q20 preserves
 * fractional-hour motion without libm or 64-bit arithmetic. Bound r <= 934
 * keeps multiplication + round bias within signed 32 bits. */
static int sine(unsigned step) {
    step %= 720u;
    if (step <= 180u) return desk_sine_q20[step];
    if (step <= 360u) return desk_sine_q20[360u - step];
    if (step <= 540u) return -desk_sine_q20[step - 360u];
    return -desk_sine_q20[720u - step];
}
static int scaled(int radius, int value) {
    int product = radius * value;
    return product < 0 ? -((-product + 524288) / 1048576) : (product + 524288) / 1048576;
}
static Point point(int cx, int cy, int radius, unsigned step) {
    Point result = {cx + scaled(radius, sine(step)), cy - scaled(radius, sine(step + 180u))};
    return result;
}
static void disk(Painter *p, int x, int y, int radius, bool black) {
    for (int dy = -radius; dy <= radius; ++dy) {
        int dx = root((uint32_t)(radius * radius - dy * dy));
        rect(p, x - dx, y + dy, 2 * dx + 1, 1, black);
    }
}
static void stroke(Painter *p, int x, int y, int xx, int yy, int width) {
    int dx = absolute(xx - x), dy = -absolute(yy - y);
    int sx = x < xx ? 1 : -1, sy = y < yy ? 1 : -1, error = dx + dy;
    for (;;) {
        disk(p, x, y, width / 2, true);
        if (x == xx && y == yy) break;
        int twice = 2 * error;
        if (twice >= dy) { error += dy; x += sx; }
        if (twice <= dx) { error += dx; y += sy; }
    }
}
static void ring(Painter *p, int x, int y, int radius, int thickness) {
    for (int dy = -radius; dy <= radius; ++dy) {
        int outer = root((uint32_t)(radius * radius - dy * dy));
        int inner_radius = radius - thickness;
        if (absolute(dy) >= inner_radius) rect(p, x - outer, y + dy, 2 * outer + 1, 1, true);
        else {
            int inner = root((uint32_t)(inner_radius * inner_radius - dy * dy));
            rect(p, x - outer, y + dy, outer - inner, 1, true);
            rect(p, x + inner + 1, y + dy, outer - inner, 1, true);
        }
    }
}
static void digit(Painter *p, int value, int x, int y, int unit) {
    static const uint8_t masks[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
    uint8_t mask = value < 0 ? 0x40 : masks[value];
    int w = 6 * unit, h = 10 * unit, gap = maximum(2, unit / 8);
    if (mask & 1) rect(p, x + unit, y, w - 2 * unit, unit - gap, true);
    if (mask & 2) rect(p, x + w - unit, y + unit, unit - gap, h / 2 - unit - gap, true);
    if (mask & 4) rect(p, x + w - unit, y + h / 2 + gap, unit - gap, h / 2 - unit - gap, true);
    if (mask & 8) rect(p, x + unit, y + h - unit, w - 2 * unit, unit - gap, true);
    if (mask & 16) rect(p, x, y + h / 2 + gap, unit - gap, h / 2 - unit - gap, true);
    if (mask & 32) rect(p, x, y + unit, unit - gap, h / 2 - unit - gap, true);
    if (mask & 64) rect(p, x + unit, y + h / 2 - unit / 2, w - 2 * unit, unit - gap, true);
}
static const DeskGlyph *glyph(int value, bool serif) {
    return &(serif ? NotoSerifGlyphs : NotoSansGlyphs)[value];
}
static void numeral(Painter *p, int value, int x, int y, int height, bool serif) {
    const DeskGlyph *g = glyph(value, serif);
    const DeskSpan *spans = serif ? NotoSerifSpans : NotoSansSpans;
    for (unsigned i = g->start; i < (unsigned)g->start + g->count; ++i) {
        const DeskSpan *s = &spans[i];
        int left = s->x * height / DESK_DIGIT_HEIGHT, top = s->y * height / DESK_DIGIT_HEIGHT;
        int w = (s->x + s->width) * height / DESK_DIGIT_HEIGHT - left;
        int h = (s->y + 1) * height / DESK_DIGIT_HEIGHT - top;
        rect(p, x + left, y + top, w, h, true);
    }
}
static int numeral_width(int value, int height, bool serif) { return glyph(value, serif)->width * height / DESK_DIGIT_HEIGHT; }
static void number(Painter *p, int value, int cx, int cy, int height, bool serif) {
    int w = numeral_width(value % 10, height, serif) + (value >= 10 ? numeral_width(value / 10, height, serif) : 0);
    int x = cx - w / 2;
    if (value >= 10) {
        numeral(p, value / 10, x, cy - height / 2, height, serif);
        x += numeral_width(value / 10, height, serif);
    }
    numeral(p, value % 10, x, cy - height / 2, height, serif);
}

bool portable_desk_draw_face(const portable_desk_canvas *canvas, unsigned face,
                             int hour24, int minute, bool use12_hour, bool valid) {
    if (!canvas || !canvas->fill_rect || canvas->width < PORTABLE_DESK_MIN_DIMENSION ||
        canvas->height < PORTABLE_DESK_MIN_DIMENSION || canvas->width > PORTABLE_DESK_MAX_DIMENSION ||
        canvas->height > PORTABLE_DESK_MAX_DIMENSION) return false;
    if (face >= PORTABLE_DESK_FACE_COUNT) face = PORTABLE_DESK_SEGMENTS;
    valid = valid && hour24 >= 0 && hour24 <= 23 && minute >= 0 && minute <= 59;
    /* Invalid inputs never reach glyph indexing or arithmetic. */
    if (!valid) { hour24 = 0; minute = 0; }
    Painter painter = {canvas, true}, *p = &painter;
    int width = canvas->width, height = canvas->height;
    rect(p, 0, 0, width, height, false);
    int hour = use12_hour ? (hour24 % 12 == 0 ? 12 : hour24 % 12) : hour24;
    if (face == PORTABLE_DESK_SEGMENTS || !valid) {
        bool short_hour=valid && hour<10;
        int columns=short_hour?22:29, shift=short_hour?7:0;
        int unit = maximum(4, minimum((width - 96) / columns, (height - 200) / 10));
        int left = (width - columns * unit) / 2, top = (height - 10 * unit) / 2;
        const int values[] = {hour / 10, hour % 10, minute / 10, minute % 10};
        const int positions[] = {0, 7, 16, 23};
        for (int i = 0; i < 4; ++i) {
            if (i == 0 && short_hour) continue;
            digit(p, valid ? values[i] : -1, left + (positions[i]-shift) * unit, top, unit);
        }
        rect(p, left + (14-shift) * unit, top + 3 * unit, unit, unit, true);
        rect(p, left + (14-shift) * unit, top + 6 * unit, unit, unit, true);
        return p->ok;
    }
    if (face == PORTABLE_DESK_SANS || face == PORTABLE_DESK_SERIF) {
        bool serif = face == PORTABLE_DESK_SERIF;
        const int values[] = {hour / 10, hour % 10, 10, minute / 10, minute % 10};
        int first = hour < 10 ? 1 : 0, natural_width = 0;
        for (int i = first; i < 5; ++i) natural_width += numeral_width(values[i], 224, serif) + 12;
        natural_width -= 12;
        int size = minimum(224, minimum(height - 200, (width - 96) * 224 / natural_width));
        int gap = 12 * size / 224, total = -gap;
        for (int i = first; i < 5; ++i) total += numeral_width(values[i], size, serif) + gap;
        int x = (width - total) / 2;
        for (int i = first; i < 5; ++i) {
            numeral(p, values[i], x, (height - size) / 2, size, serif);
            x += numeral_width(values[i], size, serif) + gap;
        }
        return p->ok;
    }
    int cx = width / 2, cy = height / 2;
    int r = minimum((height - 180) / 2, (width - 96) / 2);
    if (face != PORTABLE_DESK_MINIMAL) ring(p, cx, cy, r, face == PORTABLE_DESK_RAILWAY ? 3 : 2);
    if (face == PORTABLE_DESK_DECO) ring(p, cx, cy, r - 7, 1);
    for (unsigned tick = 0; tick < 60; ++tick) {
        bool major = tick % 5 == 0;
        if (face == PORTABLE_DESK_MINIMAL && !major) continue;
        if (face == PORTABLE_DESK_DECO && tick % 15 == 0) continue;
        int outer = r - (face == PORTABLE_DESK_MINIMAL ? 0 : 13);
        int length = major ? (face == PORTABLE_DESK_RAILWAY ? 20 : 12) : 4;
        Point a = point(cx, cy, outer, tick * 12), b = point(cx, cy, outer - length, tick * 12);
        stroke(p, a.x, a.y, b.x, b.y, major ? (face == PORTABLE_DESK_RAILWAY ? 5 : 2) : 1);
    }
    if (face == PORTABLE_DESK_DECO) {
        for (int n = 3; n <= 12; n += 3) {
            Point at = point(cx, cy, r - 30, (unsigned)n * 60);
            number(p, n, at.x, at.y, 29, true);
        }
    }
    Point h = point(cx, cy, r * 50 / 100, (unsigned)(hour24 % 12) * 60u + (unsigned)minute);
    Point m = point(cx, cy, r * 76 / 100, (unsigned)minute * 12u);
    int hour_width = face == PORTABLE_DESK_RAILWAY ? 9 : (face == PORTABLE_DESK_DECO ? 6 : 5);
    stroke(p, cx, cy, h.x, h.y, hour_width);
    stroke(p, cx, cy, m.x, m.y, face == PORTABLE_DESK_RAILWAY ? 5 : 3);
    if (face == PORTABLE_DESK_DECO) {
        Point tail = point(cx, cy, r / 7, (unsigned)(minute + 30) * 12u);
        stroke(p, cx, cy, tail.x, tail.y, 3);
        disk(p, tail.x, tail.y, 4, true);
    }
    disk(p, cx, cy, face == PORTABLE_DESK_RAILWAY ? 8 : 6, true);
    if (face == PORTABLE_DESK_MINIMAL || face == PORTABLE_DESK_DECO) disk(p, cx, cy, 2, false);
    return p->ok;
}
