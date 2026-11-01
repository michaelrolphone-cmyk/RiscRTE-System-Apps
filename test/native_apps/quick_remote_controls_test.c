/* Pure controller tests: this binary links no runtime, storage or radio code. */
#include "PortableQuickActions.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#if !defined(PORTABLE_QUICK_REMOTE_BATTERY) || !defined(PORTABLE_RESIDENT_SHELL_HOST) || \
    !defined(PORTABLE_PAPER_TRANSITIONS) || !defined(PORTABLE_QUICK_USB_TRANSFER)
#error "Remote controls tests require the resident paper Quick Actions options"
#endif

enum { CONTACT = 17, PAGE_EPOCH = 37 };
static const uint32_t remote_mask = PQA_REMOTE_OPEN | PQA_REMOTE_BACK |
    PQA_REMOTE_SCAN | PQA_REMOTE_STOP | PQA_REMOTE_SELECT |
    PQA_REMOTE_PREVIOUS | PQA_REMOTE_NEXT;

typedef struct { int x, y; uint32_t action; unsigned index; } target;
static const target page_targets[] = {
    {96, 110, PQA_REMOTE_BACK, 0},
    {240, 110, PQA_REMOTE_SCAN, 0},
    {384, 110, PQA_REMOTE_STOP, 0},
    {120, 688, PQA_REMOTE_PREVIOUS, 0},
    {360, 688, PQA_REMOTE_NEXT, 0},
    {240, 276, PQA_REMOTE_SELECT, 0},
    {240, 340, PQA_REMOTE_SELECT, 1},
    {240, 404, PQA_REMOTE_SELECT, 2},
    {240, 468, PQA_REMOTE_SELECT, 3},
    {240, 532, PQA_REMOTE_SELECT, 4},
    {240, 596, PQA_REMOTE_SELECT, 5}
};
#define TARGET_COUNT (sizeof(page_targets) / sizeof(page_targets[0]))

static pqa_state opened(bool page) {
    pqa_state s;
    pqa_init(&s);
    s.paper = true;
    pqa_set_levels(&s, true, 40, true, 70);
    s.audio_controls = true;
    s.clean_refresh_valid = true;
    s.dnd_valid = true;
    s.radio_controls = s.radios_valid = true;
    s.wifi_enabled = true;
    s.contexts_controls = s.contexts_valid = true;
#ifdef PORTABLE_FRONTLIGHT_TONE
    s.tone_controls = s.tone_valid = true;
    s.tone = 30;
#endif
    s.remote_battery_controls = true;
    s.remote_battery_page = page;
    s.remote_battery_count = 12;
    s.remote_battery_epoch = PAGE_EPOCH;
    s.remote_battery_revision = 91;
    s.position_q8 = s.target_q8 = pqa_open_q8(&s);
    return s;
}

static bool down(pqa_state *s, unsigned t, int x, int y) {
    return pqa_input(s, t, true, 1, CONTACT, x, y, true);
}
static bool up(pqa_state *s, unsigned t) {
    return pqa_input(s, t, true, 0, CONTACT, 0, 0, true);
}
static void no_action(pqa_state *s) {
    assert(pqa_take_action(s) == PQA_NONE);
}
static void unchanged_preferences(const pqa_state *s) {
    assert(s->brightness == 40 && s->volume == 70);
    assert(s->last_nonzero_brightness == 40 && s->last_nonzero_volume == 70);
    assert(s->brightness_valid && s->volume_valid && !s->torch);
    assert(s->radios_valid && s->wifi_enabled && !s->bluetooth_enabled && !s->airplane);
    assert(!s->dnd_enabled && !s->contexts_enabled && !s->error_flags);
#ifdef PORTABLE_FRONTLIGHT_TONE
    assert(s->tone == 30);
#endif
}
static void click(pqa_state *s, int x, int y) {
    assert(down(s, 100, x, y));
    no_action(s);
    assert(up(s, 110));
}
static void expect_action(pqa_state *s, uint32_t action, unsigned index) {
    assert(pqa_take_action(s) == action);
    if (action != PQA_NONE) assert(s->action_remote_epoch == s->remote_battery_epoch);
    if (action == PQA_REMOTE_SELECT) assert(s->action_remote_index == index);
    no_action(s);
    unchanged_preferences(s);
}

static void test_idle(void) {
    pqa_state s;
    pqa_init(&s);
    assert(!s.remote_battery_controls && !s.remote_battery_page);
    assert(!s.remote_battery_count && !s.remote_battery_offset);
    assert(!s.remote_battery_epoch && !s.remote_battery_revision);
    for (unsigned page = 0; page < 2; ++page) {
        s = opened(page != 0);
        for (unsigned t = 0; t < 10000; t += 10) {
            assert(up(&s, t));
            (void)pqa_animate(&s, t);
            no_action(&s);
            unchanged_preferences(&s);
        }
        pqa_close(&s);
        no_action(&s);
        pqa_cancel(&s);
        no_action(&s);
    }
}

static void test_summary(void) {
    const int xs[] = {239, 240, 241, 343, 446, 447, 448};
    const int ys[] = {743, 744, 745, 770, 794, 795, 796};
    for (unsigned ix = 0; ix < sizeof(xs) / sizeof(xs[0]); ++ix)
        for (unsigned iy = 0; iy < sizeof(ys) / sizeof(ys[0]); ++iy) {
            pqa_state s = opened(false);
            click(&s, xs[ix], ys[iy]);
            uint32_t expected = xs[ix] >= 240 && xs[ix] < 448 &&
                ys[iy] >= 744 && ys[iy] < 796 ? PQA_REMOTE_OPEN : PQA_NONE;
            expect_action(&s, expected, 0);
            assert(!s.remote_battery_page && s.remote_battery_count == 12);
        }
    pqa_state s = opened(false);
    s.remote_battery_controls = false;
    click(&s, 340, 770);
    expect_action(&s, PQA_NONE, 0);
    s = opened(true);
    click(&s, 340, 770);
    expect_action(&s, PQA_NONE, 0);
    /* Non-paper controllers do not gain the paper-only remote hit regions. */
    s = opened(false);
    s.paper = false;
    click(&s, 340, 770);
    expect_action(&s, PQA_NONE, 0);
}

static void test_explicit_intent(void) {
    for (unsigned i = 0; i < TARGET_COUNT; ++i) {
        const target *hit = &page_targets[i];
        pqa_state s = opened(true);
        for (unsigned repetition = 0; repetition < 3; ++repetition) {
            assert(down(&s, 100, hit->x, hit->y));
            assert(s.gesture == PQA_PANEL_PENDING);
            assert(s.remote_contact_epoch == PAGE_EPOCH);
            no_action(&s);
            for (unsigned t = 110; t < 1000; t += 10) {
                assert(down(&s, t, hit->x, hit->y));
                no_action(&s);
            }
            assert(up(&s, 1000));
            expect_action(&s, hit->action, hit->index);
            for (unsigned t = 1010; t < 1100; t += 10) {
                assert(up(&s, t));
                no_action(&s);
            }
            /* Actions describe intent. The adapter owns page/list state. */
            assert(s.remote_battery_page && s.remote_battery_offset == 0);
            assert(s.remote_battery_count == 12 && s.remote_battery_epoch == PAGE_EPOCH);
            assert(s.remote_battery_revision == 91);
            assert(s.position_q8 == pqa_open_q8(&s) && s.target_q8 == pqa_open_q8(&s));
        }
    }
}

/* Independent hit-map oracle, including gaps and inclusive/exclusive edges. */
static uint32_t page_hit(int x, int y, unsigned count, unsigned offset, unsigned *index) {
    if (y >= 88 && y <= 139) {
        if (x >= 32 && x <= 159) return PQA_REMOTE_BACK;
        if (x >= 176 && x <= 303) return PQA_REMOTE_SCAN;
        if (x >= 320 && x <= 447) return PQA_REMOTE_STOP;
    }
    if (y >= 664 && y <= 711) {
        if (x >= 32 && x <= 227) return PQA_REMOTE_PREVIOUS;
        if (x >= 252 && x <= 447) return PQA_REMOTE_NEXT;
    }
    if (x >= 32 && x <= 447 && y >= 248 && y <= 627) {
        unsigned row = (unsigned)(y - 248) / 64;
        if ((unsigned)(y - 248) % 64 < 60 && offset + row < count) {
            *index = offset + row;
            return PQA_REMOTE_SELECT;
        }
    }
    return PQA_NONE;
}

static void test_hit_map(void) {
    for (unsigned empty = 0; empty < 2; ++empty)
        for (int y = -1; y <= 800; ++y) for (int x = -1; x <= 480; ++x) {
        pqa_state s = opened(true);
        s.remote_battery_count = empty ? 0 : 12;
        s.remote_battery_offset = 4;
        unsigned index = 0;
        uint32_t expected = page_hit(x, y, s.remote_battery_count, s.remote_battery_offset, &index);
        click(&s, x, y);
        expect_action(&s, expected, index);
        assert(s.remote_battery_page && s.remote_battery_offset == 4);
        assert(s.remote_battery_count == (empty ? 0 : 12));
    }
}

static void test_rows_and_navigation(void) {
    const unsigned values[] = {0, 1, 5, 6, 7, 11, 12, 13, 249, 250, 254, 255};
    for (unsigned c = 0; c < sizeof(values) / sizeof(values[0]); ++c)
        for (unsigned o = 0; o < sizeof(values) / sizeof(values[0]); ++o) {
            unsigned count = values[c], offset = values[o];
            for (unsigned row = 0; row < 6; ++row) {
                pqa_state s = opened(true);
                s.remote_battery_count = (uint8_t)count;
                s.remote_battery_offset = (uint8_t)offset;
                click(&s, 240, 276 + (int)row * 64);
                expect_action(&s, offset + row < count ? PQA_REMOTE_SELECT : PQA_NONE, offset + row);
                assert(s.remote_battery_offset == offset && s.remote_battery_count == count);
            }
            for (unsigned next = 0; next < 2; ++next) {
                pqa_state s = opened(true);
                s.remote_battery_count = (uint8_t)count;
                s.remote_battery_offset = (uint8_t)offset;
                click(&s, next ? 360 : 120, 688);
                expect_action(&s, next ? PQA_REMOTE_NEXT : PQA_REMOTE_PREVIOUS, 0);
                assert(s.remote_battery_offset == offset && s.remote_battery_count == count);
            }
        }
}

static void test_drag_cancel(void) {
    const int motion[][2] = {{7, 0}, {-7, 0}, {0, 7}, {0, -7}, {120, 120}, {-120, -120}};
    for (unsigned i = 0; i <= TARGET_COUNT; ++i)
        for (unsigned d = 0; d < sizeof(motion) / sizeof(motion[0]); ++d) {
            const target summary = {340, 770, PQA_REMOTE_OPEN, 0};
            const target *hit = i == TARGET_COUNT ? &summary : &page_targets[i];
            pqa_state s = opened(i != TARGET_COUNT);
            assert(down(&s, 100, hit->x, hit->y));
            assert(down(&s, 110, hit->x + motion[d][0], hit->y + motion[d][1]));
            assert(s.moved);
            assert(down(&s, 120, hit->x, hit->y));
            assert(up(&s, 130));
            expect_action(&s, PQA_NONE, 0);
        }
    /* Motion at, rather than beyond, the six-pixel threshold remains a tap. */
    pqa_state s = opened(true);
    assert(down(&s, 100, 240, 110));
    assert(down(&s, 110, 246, 116));
    assert(up(&s, 120));
    expect_action(&s, PQA_REMOTE_SCAN, 0);
}

static void test_cancel_and_gate(void) {
    for (unsigned i = 0; i < TARGET_COUNT; ++i) for (unsigned loss = 0; loss < 6; ++loss) {
        const target *hit = &page_targets[i];
        pqa_state s = opened(true);
        assert(down(&s, 100, hit->x, hit->y));
        if (loss == 0) assert(pqa_input(&s, 110, false, 0, 0, 0, 0, true));
        if (loss == 1) assert(pqa_input(&s, 110, false, 1, CONTACT, hit->x, hit->y, true));
        if (loss == 2) assert(pqa_input(&s, 110, true, 2, CONTACT, hit->x, hit->y, true));
        if (loss == 3) assert(pqa_input(&s, 110, true, 1, CONTACT + 1, hit->x, hit->y, true));
        if (loss == 4) pqa_cancel_input(&s);
        if (loss == 5) pqa_close(&s);
        assert(s.gesture == PQA_IDLE && s.neutral_gate);
        no_action(&s);
        assert(down(&s, 120, hit->x, hit->y));
        assert(s.neutral_gate);
        no_action(&s);
        assert(pqa_input(&s, 130, false, 0, 0, 0, 0, true));
        assert(s.neutral_gate);
        assert(up(&s, 140));
        assert(!s.neutral_gate);
        expect_action(&s, PQA_NONE, 0);
        s.position_q8 = s.target_q8 = pqa_open_q8(&s);
        click(&s, hit->x, hit->y);
        expect_action(&s, hit->action, hit->index);
    }
}

static void test_epoch_and_snapshot(void) {
    for (unsigned i = 0; i <= TARGET_COUNT; ++i) {
        const target summary = {340, 770, PQA_REMOTE_OPEN, 0};
        const target *hit = i == TARGET_COUNT ? &summary : &page_targets[i];
        pqa_state s = opened(i != TARGET_COUNT);
        assert(down(&s, 100, hit->x, hit->y));
        ++s.remote_battery_epoch;
        assert(up(&s, 110));
        expect_action(&s, PQA_NONE, 0);
        click(&s, hit->x, hit->y);
        expect_action(&s, hit->action, hit->index);
    }
    pqa_state s = opened(true);
    s.remote_battery_epoch = UINT32_MAX;
    assert(down(&s, 100, 240, 276));
    s.remote_battery_epoch = 0;
    assert(up(&s, 110));
    expect_action(&s, PQA_NONE, 0);
    s = opened(true);
    s.remote_battery_offset = 4;
    click(&s, 240, 404);
    assert(s.action_remote_epoch == PAGE_EPOCH && s.action_remote_index == 6);
    ++s.remote_battery_epoch;
    s.remote_battery_offset = 0;
    assert(pqa_take_action(&s) == PQA_REMOTE_SELECT);
    assert(s.action_remote_epoch == PAGE_EPOCH && s.action_remote_index == 6);
    no_action(&s);
}

static void test_closed_and_moving(void) {
    for (unsigned i = 0; i <= TARGET_COUNT; ++i) for (unsigned phase = 0; phase < 5; ++phase) {
        const target summary = {340, 770, PQA_REMOTE_OPEN, 0};
        const target *hit = i == TARGET_COUNT ? &summary : &page_targets[i];
        pqa_state s = opened(i != TARGET_COUNT);
        const int32_t open = pqa_open_q8(&s);
        if (phase == 0) s.position_q8 = s.target_q8 = 0;
        if (phase == 1) { s.position_q8 = 0; s.target_q8 = open; }
        if (phase == 2) s.position_q8 = open / 2;
        if (phase == 3) s.position_q8 = open - 1;
        if (phase == 4) s.target_q8 = 0;
        bool consumed = down(&s, 100, hit->x, hit->y);
        assert(consumed == (phase != 0));
        no_action(&s);
        /* Settling after the press cannot retroactively activate its target. */
        s.position_q8 = s.target_q8 = open;
        (void)up(&s, 110);
        expect_action(&s, PQA_NONE, 0);
    }
}

static void test_disabled_controls(void) {
    for (unsigned page = 0; page < 2; ++page) for (unsigned i = 0; i < TARGET_COUNT; ++i) {
        pqa_state s = opened(page != 0);
        s.remote_battery_controls = false;
        const target *hit = &page_targets[i];
        assert(down(&s, 100, hit->x, hit->y));
        assert(up(&s, 110));
        assert(!(pqa_take_action(&s) & remote_mask));
    }
    /* On the ordinary sheet, page-only hits must never emit remote intent. */
    for (unsigned i = 0; i < TARGET_COUNT; ++i) {
        pqa_state s = opened(false);
        const target *hit = &page_targets[i];
        assert(down(&s, 100, hit->x, hit->y));
        assert(up(&s, 110));
        assert(!(pqa_take_action(&s) & remote_mask));
    }
}

static void test_page_invalidated(void) {
    for (unsigned i = 0; i < TARGET_COUNT; ++i) {
        pqa_state s = opened(true);
        const target *hit = &page_targets[i];
        assert(down(&s, 100, hit->x, hit->y));
        s.remote_battery_page = false;
        assert(up(&s, 110));
        expect_action(&s, PQA_NONE, 0);
    }
    pqa_state s = opened(true);
    assert(down(&s, 100, 240, 110));
    s.remote_battery_controls = false;
    assert(up(&s, 110));
    expect_action(&s, PQA_NONE, 0);
    s = opened(false);
    assert(down(&s, 100, 340, 770));
    s.remote_battery_page = true;
    assert(up(&s, 110));
    expect_action(&s, PQA_NONE, 0);
}

static void test_selection_invalidated(void) {
    pqa_state s = opened(true);
    assert(down(&s, 100, 240, 596));
    s.remote_battery_count = 5;
    assert(up(&s, 110));
    expect_action(&s, PQA_NONE, 0);
    s = opened(true);
    assert(down(&s, 100, 240, 596));
    s.remote_battery_offset = 255;
    assert(up(&s, 110));
    expect_action(&s, PQA_NONE, 0);
}

typedef void (*test_fn)(void);
static const struct { const char *name; test_fn run; } tests[] = {
    {"idle", test_idle}, {"summary", test_summary},
    {"explicit-intent", test_explicit_intent}, {"hit-map", test_hit_map},
    {"rows-navigation", test_rows_and_navigation}, {"drag-cancel", test_drag_cancel},
    {"cancel-gate", test_cancel_and_gate}, {"epoch-snapshot", test_epoch_and_snapshot},
    {"closed-moving", test_closed_and_moving}, {"disabled", test_disabled_controls},
    {"page-invalidated", test_page_invalidated},
    {"selection-invalidated", test_selection_invalidated}
};

int main(int argc, char **argv) {
    assert(argc <= 2);
    unsigned ran = 0;
    for (unsigned i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i)
        if (argc == 1 || !strcmp(argv[1], tests[i].name)) {
            tests[i].run();
            printf("Remote Quick Actions: %s passed\n", tests[i].name);
            ++ran;
        }
    assert(ran);
    return 0;
}
