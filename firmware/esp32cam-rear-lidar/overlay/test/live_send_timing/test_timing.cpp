#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>

ssize_t timingTestSend(int descriptor, const void *data, size_t size, int flags);
// Replace only the syscall boundary; compile the actual production writer,
// including its inherited control dispatch and all early-return paths.
#define send timingTestSend
#include "live_websocket.h"
#undef send

uint64_t transportTestClockUs = 1;
uint64_t timingTestLoopCostUs = 11;
uint64_t timingTestDelayExtraUs = 500;

namespace {
struct SendStep { ssize_t count; int error; uint64_t elapsed; };
std::deque<SendStep> steps;
bool alwaysWouldBlock = false;
std::vector<uint8_t> acceptedWire;
uint64_t actualCalls = 0;
constexpr ssize_t allBytes = -2;

struct Pair {
    int sockets[2];
    Pair() { assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0); }
    ~Pair() { ::close(sockets[1]); }
};

void reset() {
    transportTestClockUs = 1;
    timingTestLoopCostUs = 11;
    timingTestDelayExtraUs = 500;
    steps.clear(); alwaysWouldBlock = false; acceptedWire.clear(); actualCalls = 0;
}

void accounting(const LiveSendTiming &t) {
    const LiveSendCounters &c = t.counters;
    assert(t.accounting_valid);
    assert(c.total_us == t.completed_us - t.started_us);
    assert(c.total_us == c.loop_us + c.fragment_yield_us + c.send_syscall_us +
                         c.send_wait_us + c.other_us);
}

LiveSendTiming fragmentedSuccess(bool measured, std::vector<uint8_t> *wire = nullptr) {
    reset();
    Pair pair; TestTcp tcp(pair.sockets[0]); LiveWebSocket socket; socket.attach(tcp);
    std::vector<uint8_t> frame(WEBSOCKETS_MAX_HEADER_SIZE + 3000, 0x5a);
    LiveSendTiming timing;
    assert(socket.writeLive(WSop_binary, frame.data(), 3000, true, measured ? &timing : nullptr));
    assert(socket.disconnects == 0 && socket.fallbackWrites == 0);
    if (wire) *wire = acceptedWire;
    if (!measured) return timing;
    accounting(timing);
    const LiveSendCounters &c = timing.counters;
    assert(timing.delivered && c.payload_bytes == 3000);
    assert(c.completed_payload_bytes == 3000 && c.wire_bytes == 3024);
    assert(c.fragments_attempted == 3 && c.fragments_completed == 3);
    assert(c.loop_calls == 3 && c.loop_us == 33);
    assert(c.fragment_yields == 2 && c.fragment_yield_us == 3000);
    assert(c.send_calls == 3 && c.send_syscall_us == 21);
    assert(c.send_waits == 0 && c.send_wait_us == 0 && c.other_us == 0);
    assert(c.total_us == 3054);
    return timing;
}

LiveSendTiming partialWriteAndRetries() {
    reset();
    steps = {{10, 0, 7}, {-1, EAGAIN, 13}, {-1, EINTR, 5}, {allBytes, 0, 11}};
    Pair pair; TestTcp tcp(pair.sockets[0]); LiveWebSocket socket; socket.attach(tcp);
    std::vector<uint8_t> frame(WEBSOCKETS_MAX_HEADER_SIZE + 128, 0x5a);
    LiveSendTiming timing;
    assert(socket.writeLive(WSop_binary, frame.data(), 128, true, &timing));
    accounting(timing);
    const LiveSendCounters &c = timing.counters;
    assert(timing.delivered && c.payload_bytes == 128 && c.completed_payload_bytes == 128);
    assert(c.wire_bytes == 136 && c.send_calls == 4 && c.send_syscall_us == 36);
    assert(c.would_block == 1 && c.interrupted == 1 && c.send_waits == 2);
    assert(c.send_wait_us == 3000 && c.fragment_yields == 0);
    assert(c.loop_us == 11 && c.total_us == 3047);
    return timing;
}

void inheritedControlIsCountedOnceInLoop() {
    reset();
    steps = {{-1, EAGAIN, 13}, {allBytes, 0, 11}, {allBytes, 0, 7}};
    Pair pair; TestTcp tcp(pair.sockets[0]); LiveWebSocket socket; socket.attach(tcp);
    socket.scheduleHeartbeat();
    uint8_t frame[WEBSOCKETS_MAX_HEADER_SIZE + 32] = {};
    LiveSendTiming timing;
    assert(socket.writeLive(WSop_binary, frame, 32, true, &timing));
    accounting(timing);
    const LiveSendCounters &c = timing.counters;
    assert(socket.controlBytes == 6 && actualCalls == 3);
    assert(c.loop_us == 1535 && c.loop_calls == 1); // 11 + 13 + 1500 + 11.
    assert(c.send_calls == 1 && c.send_syscall_us == 7 && c.wire_bytes == 38);
    assert(c.would_block == 0 && c.send_waits == 0 && c.send_wait_us == 0);
    assert(c.total_us == 1542 && acceptedWire.size() == 44);
}

LiveSendTiming partialDataTimeout() {
    reset();
    steps = {{10, 0, 7}};
    alwaysWouldBlock = true;
    Pair pair; TestTcp tcp(pair.sockets[0]); LiveWebSocket socket; socket.attach(tcp);
    uint8_t frame[WEBSOCKETS_MAX_HEADER_SIZE + 32] = {};
    LiveSendTiming timing;
    assert(!socket.writeLive(WSop_binary, frame, 32, true, &timing));
    accounting(timing);
    const LiveSendCounters &c = timing.counters;
    assert(!timing.delivered && socket.disconnects == 1 && socket.writeTimeoutCount() == 1);
    assert(c.wire_bytes == 10 && c.completed_payload_bytes == 0);
    assert(c.fragments_attempted == 1 && c.fragments_completed == 0);
    assert(c.send_calls == c.would_block + 1 && c.send_waits + 1 == c.would_block);
    assert(c.send_wait_us == c.send_waits * 1500);
    assert(c.total_us >= LiveWriteProgress::idleLimitUs);
    assert(c.total_us < LiveWriteProgress::idleLimitUs + 1600);
    return timing;
}

void inheritedControlTimeoutPublishesFailureWithoutDataCounts() {
    reset();
    alwaysWouldBlock = true;
    Pair pair; TestTcp tcp(pair.sockets[0]); LiveWebSocket socket; socket.attach(tcp);
    socket.scheduleHeartbeat();
    uint8_t frame[WEBSOCKETS_MAX_HEADER_SIZE + 32] = {};
    LiveSendTiming timing;
    assert(!socket.writeLive(WSop_binary, frame, 32, true, &timing));
    accounting(timing);
    const LiveSendCounters &c = timing.counters;
    assert(!timing.delivered && socket.disconnects == 1 && socket.writeTimeoutCount() == 1);
    assert(c.total_us == c.loop_us && c.loop_us >= LiveWriteProgress::idleLimitUs);
    assert(c.send_calls == 0 && c.send_syscall_us == 0 && c.send_wait_us == 0);
    assert(c.wire_bytes == 0 && c.fragments_attempted == 0);
}

void disconnectedAttemptReplacesPreviousResult() {
    reset();
    LiveWebSocket socket;
    LiveSendTiming timing;
    timing.delivered = true; timing.counters.wire_bytes = 999;
    uint8_t frame[WEBSOCKETS_MAX_HEADER_SIZE + 32] = {};
    assert(!socket.writeLive(WSop_binary, frame, 32, true, &timing));
    accounting(timing);
    assert(!timing.delivered && timing.counters.payload_bytes == 32);
    assert(timing.counters.wire_bytes == 0 && timing.counters.total_us == 0);
}

void cumulativeSnapshotsIncludeFailedAttempts() {
    const LiveSendTiming success = fragmentedSuccess(true);
    const LiveSendTiming retry = partialWriteAndRetries();
    const LiveSendTiming failure = partialDataTimeout();
    LiveSendTimingSnapshot state;
    state.record(success);
    const LiveSendTimingSnapshot saved = state;
    state.record(retry); state.record(failure);
    assert(saved.attempts == 1 && saved.delivered == 1 && saved.failed == 0);
    assert(saved.cumulative.total_us == success.counters.total_us);
    assert(state.attempts == 3 && state.delivered == 2 && state.failed == 1);
    assert(state.invalid_accounting == 0 && !state.last.delivered);
    assert(state.cumulative.total_us == success.counters.total_us +
           retry.counters.total_us + failure.counters.total_us);
    assert(state.cumulative.wire_bytes == 3024 + 136 + 10);
}

void instrumentationPreservesWireAndPacing() {
    std::vector<uint8_t> measured, plain;
    fragmentedSuccess(true, &measured);
    const uint64_t measuredElapsed = transportTestClockUs, measuredCalls = actualCalls;
    fragmentedSuccess(false, &plain);
    assert(measured == plain && transportTestClockUs == measuredElapsed);
    assert(actualCalls == measuredCalls);
}
} // namespace

ssize_t timingTestSend(int, const void *data, size_t size, int flags) {
    assert(flags == MSG_DONTWAIT);
    ++actualCalls;
    SendStep step{allBytes, 0, 7};
    if (!steps.empty()) { step = steps.front(); steps.pop_front(); }
    else if (alwaysWouldBlock) step = {-1, EAGAIN, 7};
    transportTestClockUs += step.elapsed;
    const ssize_t count = step.count == allBytes ? static_cast<ssize_t>(size) : step.count;
    assert(count <= static_cast<ssize_t>(size));
    if (count > 0) {
        const uint8_t *bytes = static_cast<const uint8_t *>(data);
        acceptedWire.insert(acceptedWire.end(), bytes, bytes + count);
    }
    errno = step.error;
    return count;
}

int main() {
    static_assert(LiveWriteProgress::idleLimitUs == 4000000, "Use materialized rear budget");
    fragmentedSuccess(true); puts("PASS fragmented elapsed yield and syscall accounting");
    partialWriteAndRetries(); puts("PASS partial writes and distinct EAGAIN/EINTR waits");
    inheritedControlIsCountedOnceInLoop(); puts("PASS nested heartbeat counted once in loop");
    partialDataTimeout(); puts("PASS partial data timeout produces bounded failed record");
    inheritedControlTimeoutPublishesFailureWithoutDataCounts();
    puts("PASS control timeout excludes data buckets");
    disconnectedAttemptReplacesPreviousResult(); puts("PASS early return clears stale result");
    cumulativeSnapshotsIncludeFailedAttempts(); puts("PASS cumulative success and failure snapshots");
    instrumentationPreservesWireAndPacing(); puts("PASS enabled and unmeasured wire/pacing equivalence");
    puts("8 production sender timing groups passed");
}
