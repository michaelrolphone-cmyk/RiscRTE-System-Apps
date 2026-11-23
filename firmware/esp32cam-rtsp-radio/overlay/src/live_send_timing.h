#pragma once
#include <cstddef>
#include <cstdint>
#include <esp_timer.h>

// Elapsed wall time, including descheduling inside each measured operation.
// Buckets are disjoint: inherited control writes belong to loop_us, never to
// the data-write syscall/retry buckets. Nothing here controls transport timing.
struct LiveSendCounters {
    uint64_t total_us = 0, loop_us = 0, fragment_yield_us = 0;
    uint64_t send_syscall_us = 0, send_wait_us = 0, other_us = 0;
    uint64_t payload_bytes = 0, completed_payload_bytes = 0, wire_bytes = 0;
    uint64_t loop_calls = 0, fragments_attempted = 0, fragments_completed = 0;
    uint64_t send_calls = 0, would_block = 0, interrupted = 0;
    uint64_t send_waits = 0, fragment_yields = 0;

    void add(const LiveSendCounters &value) {
#define LIVE_SEND_ADD(field) field += value.field
        LIVE_SEND_ADD(total_us); LIVE_SEND_ADD(loop_us);
        LIVE_SEND_ADD(fragment_yield_us); LIVE_SEND_ADD(send_syscall_us);
        LIVE_SEND_ADD(send_wait_us); LIVE_SEND_ADD(other_us);
        LIVE_SEND_ADD(payload_bytes); LIVE_SEND_ADD(completed_payload_bytes);
        LIVE_SEND_ADD(wire_bytes); LIVE_SEND_ADD(loop_calls);
        LIVE_SEND_ADD(fragments_attempted); LIVE_SEND_ADD(fragments_completed);
        LIVE_SEND_ADD(send_calls); LIVE_SEND_ADD(would_block);
        LIVE_SEND_ADD(interrupted); LIVE_SEND_ADD(send_waits);
        LIVE_SEND_ADD(fragment_yields);
#undef LIVE_SEND_ADD
    }
};

struct LiveSendTiming {
    LiveSendCounters counters;
    uint64_t capture_us = 0, started_us = 0, completed_us = 0;
    bool delivered = false, accounting_valid = false;
};

// The owner publishes a completed attempt under its short snapshot lock.
// An interrupted/in-progress attempt is not presented as a completed sample.
struct LiveSendTimingSnapshot {
    LiveSendTiming last;
    LiveSendCounters cumulative;
    uint64_t attempts = 0, delivered = 0, failed = 0, invalid_accounting = 0;

    void record(const LiveSendTiming &value) {
        last = value;
        cumulative.add(value.counters);
        ++attempts;
        if (value.delivered) ++delivered;
        else ++failed;
        if (!value.accounting_valid) ++invalid_accounting;
    }
};

// All early returns finalize the caller-owned result. A null result performs
// no clock reads; inherited control writes use that unmeasured path.
class LiveSendAttempt {
public:
    LiveSendAttempt(LiveSendTiming *result, size_t bytes) : result_(result) {
        if (!result_) return;
        *result_ = LiveSendTiming{};
        result_->counters.payload_bytes = bytes;
        result_->started_us = esp_timer_get_time();
    }
    ~LiveSendAttempt() {
        if (!result_) return;
        result_->completed_us = esp_timer_get_time();
        LiveSendCounters &c = result_->counters;
        c.total_us = result_->completed_us - result_->started_us;
        const uint64_t measured = c.loop_us + c.fragment_yield_us +
                                 c.send_syscall_us + c.send_wait_us;
        result_->accounting_valid = measured <= c.total_us;
        c.other_us = result_->accounting_valid ? c.total_us - measured : 0;
        result_->delivered = delivered_;
    }
    void delivered() { delivered_ = true; }
    LiveSendAttempt(const LiveSendAttempt &) = delete;
    LiveSendAttempt &operator=(const LiveSendAttempt &) = delete;
private:
    LiveSendTiming *result_;
    bool delivered_ = false;
};
