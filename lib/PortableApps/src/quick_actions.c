#include "PortableQuickActions.h"
#include <string.h>

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static int absi(int v) { return v < 0 ? -v : v; }
static void emit_brightness(pqa_state *s, bool commit) {
    if (commit) {
        s->pending &= ~(uint32_t)PQA_BRIGHTNESS_PREVIEW;
        s->pending |= PQA_BRIGHTNESS_COMMIT;
        s->action_brightness = s->brightness;
    } else if (!(s->pending & PQA_BRIGHTNESS_COMMIT)) {
        s->pending |= PQA_BRIGHTNESS_PREVIEW;
        s->action_brightness = s->brightness;
    }
}
static void emit_volume(pqa_state *s, uint32_t action) {
    s->action_volume = s->volume;
    s->pending |= action;
}
static void emit_torch(pqa_state *s) {
    s->action_torch = s->torch;
    s->pending |= PQA_TORCH;
}
void pqa_init(pqa_state *s) {
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->brightness = 40;
    s->volume = 50;
    s->last_nonzero_volume = 50;
    s->pressed_tile = -1;
}
void pqa_set_levels(pqa_state *s, bool bv, unsigned b, bool vv, unsigned v) {
    if (!s) return;
    s->brightness_valid = bv;
    s->volume_valid = vv;
    s->brightness = (uint8_t)(b > 100u ? 100u : b);
    s->volume = (uint8_t)(v > 100u ? 100u : v);
    if (s->volume) s->last_nonzero_volume = s->volume;
}
bool pqa_visible(const pqa_state *s) {
    return s && (s->position_q8 > 0 || s->target_q8 > 0 || s->torch);
}
bool pqa_capture(const pqa_state *s) {
    return s && (s->neutral_gate || (s->gesture != PQA_IDLE && s->gesture != PQA_PASS_THROUGH));
}
uint32_t pqa_take_action(pqa_state *s) {
    uint32_t action = s ? s->pending : 0;
    if (s) s->pending = 0;
    return action;
}
void pqa_cancel_input(pqa_state *s) {
    if (!s) return;
    if (s->gesture == PQA_BRIGHTNESS_DRAG && s->brightness != s->saved_brightness) {
        s->brightness = s->saved_brightness;
        emit_brightness(s, false);
    }
    if (s->gesture == PQA_VOLUME_DRAG) {
        s->volume = s->saved_volume;
        s->last_nonzero_volume = s->saved_last_nonzero_volume;
    }
    s->gesture = PQA_IDLE;
    s->pressed_tile = -1;
    s->neutral_gate = true;
    s->velocity_q8 = 0;
    s->release_velocity_q8 = 0;
    s->route = PQA_CONSUMED;
}
void pqa_close(pqa_state *s) {
    if (!s) return;
    pqa_cancel_input(s);
    s->target_q8 = 0;
    if (s->torch) { s->torch = false; emit_torch(s); }
}
void pqa_cancel(pqa_state *s) {
    if (!s) return;
    pqa_close(s);
    s->position_q8 = 0;
}
static int tile_at(const pqa_state *s,int x, int y) {
    if(s->contexts_controls&&x>=20&&x<220&&y>=182&&y<222)return 9;
    if(s->contexts_controls)y+=30;
    static const int xs[3] = {20, 91, 162};
    for (int row = 0; row != 2; ++row)
        for (int col = 0; col != 3; ++col)
            if (x >= xs[col] && x < xs[col] + 62 &&
                y >= 118 + row * 48 && y < 160 + row * 48)
                return row * 3 + col;
    return -1;
}
static unsigned slider_value(int x, unsigned minimum) {
    int value = ((clampi(x, 44, 182) - 44) * 10 + 69) / 138;
    return (unsigned)clampi(value * 10, (int)minimum, 100);
}
static void slide(pqa_state *s, int x) {
    if (s->gesture == PQA_BRIGHTNESS_DRAG) {
        unsigned b = slider_value(x, 10);
        if (b != s->brightness) { s->brightness = (uint8_t)b; emit_brightness(s, false); }
    } else {
        s->volume = (uint8_t)slider_value(x, 0);
    }
}
static void begin_contact(pqa_state *s, uint32_t now, uint32_t id, int x, int y) {
    s->start_ms = s->last_ms = now;
    s->start_id = id;
    s->start_x = s->last_x = (int16_t)x;
    s->start_y = s->last_y = (int16_t)y;
    s->start_position_q8 = s->position_q8;
    s->release_velocity_q8 = 0;
    s->pressed_tile = -1;
    s->handle_pressed = false;
    s->moved = false;
    s->saved_brightness = s->brightness;
    s->saved_volume = s->volume;
    s->saved_last_nonzero_volume = s->last_nonzero_volume;
}
static bool route(pqa_state *s, pqa_route r) {
    s->route = r;
    return r == PQA_RESERVED || r == PQA_CONSUMED;
}
static void release_panel(pqa_state *s, uint32_t now) {
    int velocity = (now - s->last_ms > 100u) ? 0 : s->release_velocity_q8;
    bool open = s->position_q8 > PQA_OPEN_Q8 / 2 ? velocity > -384 : velocity > 512;
    s->target_q8 = open ? PQA_OPEN_Q8 : 0;
    s->velocity_q8 = velocity / 2;
}
bool pqa_input(pqa_state *s, uint32_t now, bool valid, unsigned count,
               uint32_t id, int x, int y, bool allow_open) {
    if (!s) return false;
    /* Bound hostile/raw coordinates before subtraction or Q8 conversion. */
    x = clampi(x, -1024, 1024); y = clampi(y, -1024, 1024);
    if (!valid || count > 1u) {
        if (s->gesture == PQA_PANEL_DRAG)
            s->target_q8 = s->position_q8 >= PQA_OPEN_Q8 / 2 ? PQA_OPEN_Q8 : 0;
        pqa_cancel_input(s);
        return route(s, PQA_CONSUMED);
    }
    if (s->neutral_gate) {
        if (!count) s->neutral_gate = false;
        return route(s, PQA_CONSUMED);
    }
    if (!count) {
        pqa_gesture previous = s->gesture;
        s->gesture = PQA_IDLE;
        if (previous == PQA_TOP_PENDING) return route(s, PQA_REPLAY);
        if (previous == PQA_PASS_THROUGH || previous == PQA_IDLE)
            return route(s, pqa_visible(s) ? PQA_CONSUMED : PQA_PASS);
        if (previous == PQA_PANEL_DRAG) release_panel(s, now);
        else if (previous == PQA_BRIGHTNESS_DRAG) emit_brightness(s, true);
        else if (previous == PQA_VOLUME_DRAG) {
            if (s->volume) s->last_nonzero_volume = s->volume;
            emit_volume(s, PQA_VOLUME_COMMIT);
        } else if (previous == PQA_TORCH_TAP) {
            if (!s->moved) { s->torch = false; emit_torch(s); }
        } else if (previous == PQA_PANEL_PENDING && !s->moved) {
            if (s->handle_pressed) s->target_q8 = 0;
            else if (s->pressed_tile == 0 && s->volume_valid) {
                if (s->volume) { s->last_nonzero_volume = s->volume; s->volume = 0; }
                else s->volume = s->last_nonzero_volume ? s->last_nonzero_volume : 50;
                emit_volume(s, PQA_SILENT);
            } else if(s->pressed_tile==9 && s->contexts_controls && s->contexts_valid) {
                s->action_contexts=!s->contexts_enabled;s->pending|=PQA_CONTEXTS;
            } else if(s->pressed_tile==1 && s->dnd_valid) {
                s->dnd_enabled=!s->dnd_enabled;
                s->action_dnd=s->dnd_enabled;s->pending|=PQA_DND;
            } else if(s->pressed_tile==2 && s->radios_valid) {
                s->pending|=PQA_AIRPLANE;
            } else if(s->pressed_tile==4 && s->radios_valid) {
                s->pending|=PQA_BLUETOOTH;
            } else if (s->pressed_tile == 3 && (!s->radio_controls || s->radios_valid)) {
                s->pending |= PQA_WIFI;if(!s->radio_controls)s->target_q8=0;
            } else if (s->pressed_tile == 5) {
                s->torch = true; s->target_q8 = 0; emit_torch(s);
            }
        }
        return route(s, PQA_CONSUMED);
    }
    if (s->gesture != PQA_IDLE && id != s->start_id) {
        pqa_cancel_input(s);
        return route(s, PQA_CONSUMED);
    }
    if (s->gesture == PQA_PASS_THROUGH) return route(s, PQA_PASS);
    if (s->gesture == PQA_IDLE) {
        begin_contact(s, now, id, x, y);
        if (s->torch) s->gesture = PQA_TORCH_TAP;
        else if (!pqa_visible(s)) {
            if (allow_open && y >= 0 && y < PQA_TOP_EDGE && x >= 0 && x < 240)
                s->gesture = PQA_TOP_PENDING;
            else s->gesture = PQA_PASS_THROUGH;
        } else {
            /* Moving panels are draggable but never activate controls. */
            int local_y = y + (PQA_OPEN_Q8 - s->position_q8) / 256;
            bool settled = s->position_q8 == PQA_OPEN_Q8 && s->target_q8 == PQA_OPEN_Q8;
            if (settled && x >= 16 && x < 224 && local_y >= (s->contexts_controls?24:48) && local_y < (s->contexts_controls?53:77) && s->brightness_valid)
                s->gesture = PQA_BRIGHTNESS_DRAG;
            else if (settled && x >= 16 && x < 224 && local_y >= (s->contexts_controls?54:78) && local_y < (s->contexts_controls?83:107) && s->volume_valid)
                s->gesture = PQA_VOLUME_DRAG;
            else {
                s->gesture = PQA_PANEL_PENDING;
                if (settled) {
                    s->pressed_tile = (int8_t)tile_at(s,x, local_y);
                    s->handle_pressed = x >= 76 && x < 164 && local_y >= (s->contexts_controls?222:207) && local_y < 239;
                }
            }
        }
        if (s->gesture == PQA_BRIGHTNESS_DRAG || s->gesture == PQA_VOLUME_DRAG) slide(s, x);
        return route(s, s->gesture == PQA_PASS_THROUGH ? PQA_PASS :
                       s->gesture == PQA_TOP_PENDING ? PQA_RESERVED : PQA_CONSUMED);
    }
    int dx = x - s->start_x, dy = y - s->start_y;
    if (absi(dx) > 6 || absi(dy) > 6) s->moved = true;
    if (s->gesture == PQA_TOP_PENDING) {
        if (dy > 6 && dy > absi(dx)) s->gesture = PQA_PANEL_DRAG;
        else if (absi(dx) > 6 || dy < -6) {
            s->gesture = PQA_PASS_THROUGH;
            return route(s, PQA_REPLAY);
        } else return route(s, PQA_RESERVED);
    } else if (s->gesture == PQA_PANEL_PENDING && absi(dy) > 6 && absi(dy) > absi(dx)) {
        s->gesture = PQA_PANEL_DRAG;
    }
    if (s->gesture == PQA_PANEL_DRAG) {
        s->position_q8 = clampi(s->start_position_q8 + dy * 256, 0, PQA_OPEN_Q8);
        uint32_t elapsed = now - s->last_ms;
        if (elapsed) s->release_velocity_q8 = clampi((y - s->last_y) * 2048 / (int)(elapsed > 1000u ? 1000u : elapsed), -8192, 8192);
        s->velocity_q8 = 0;
        s->last_y = (int16_t)y; s->last_x = (int16_t)x; s->last_ms = now;
    } else if (s->gesture == PQA_BRIGHTNESS_DRAG || s->gesture == PQA_VOLUME_DRAG) slide(s, x);
    return route(s, PQA_CONSUMED);
}
bool pqa_animate(pqa_state *s, uint32_t now) {
    if (!s) return false;
    if (!s->clock_valid) { s->clock_valid = true; s->animation_ms = now; return false; }
    uint32_t elapsed = now - s->animation_ms;
    s->animation_ms = now;
    /* A long suspension resolves to the target without an unbounded catch-up. */
    if (elapsed > 1000u && s->gesture != PQA_PANEL_DRAG) {
        bool changed = s->position_q8 != s->target_q8;
        s->position_q8 = s->target_q8; s->velocity_q8 = 0; s->animation_remainder_ms = 0;
        return changed;
    }
    s->animation_remainder_ms += elapsed > 1000u ? 1000u : elapsed;
    unsigned steps = s->animation_remainder_ms / PQA_ANIMATION_STEP_MS;
    s->animation_remainder_ms %= PQA_ANIMATION_STEP_MS;
    if (s->gesture == PQA_PANEL_DRAG || s->gesture == PQA_TOP_PENDING) return false;
    int32_t before = s->position_q8;
    while (steps--) {
        int delta = s->target_q8 - s->position_q8;
        s->velocity_q8 = (s->velocity_q8 + delta / 18) * 3 / 4;
        s->position_q8 = clampi(s->position_q8 + s->velocity_q8, 0, PQA_OPEN_Q8);
        if (absi(delta) < 64 && absi(s->velocity_q8) < 64) {
            s->position_q8 = s->target_q8; s->velocity_q8 = 0;
        }
        if ((s->position_q8 == 0 && s->velocity_q8 < 0) ||
            (s->position_q8 == PQA_OPEN_Q8 && s->velocity_q8 > 0)) s->velocity_q8 = 0;
    }
    return s->position_q8 != before;
}
