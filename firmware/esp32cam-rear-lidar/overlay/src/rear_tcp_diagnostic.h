#pragma once

#include <atomic>
#include <cstdint>
#include <cerrno>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <lwip/sockets.h>
#include <lwip/tcpip.h>
#include <lwip/priv/tcp_priv.h>

// This adapter is instantiated only in the rear package. Its contexts and two
// callback messages live until reboot; never put a begun context on a stack,
// copy it, delete it, or cancel/reuse an outstanding request on disconnect.
class RearTcpDiagnostic {
public:
    static constexpr uint64_t intervalUs = 250000;
    static constexpr uint64_t staleAfterUs = 1000000;
    static constexpr unsigned maxPcbs = 32, maxSegments = 16;
    static_assert(TCP_SLOW_INTERVAL == 500, "Re-review TCP timer units");
    static_assert(TCP_MSS == 1436 && TCP_SND_BUF == 5760, "Re-review SDK TCP profile");

    struct Tuple {
        uint32_t localIp = 0, remoteIp = 0; // IPv4 network order.
        uint16_t localPort = 0, remotePort = 0; // Host order.
    };
    struct Segments {
        uint32_t payload_bytes = 0, first_seq = 0, last_end_seq = 0;
        uint16_t visited = 0;
        bool truncated = false, not_visited = false;
    };
    struct Sample {
        uint64_t generation = 0, requested_us = 0, started_us = 0, completed_us = 0;
        bool would_block = false, found = false, pcb_walk_truncated = false;
        uint16_t pcbs_visited = 0;
        uint32_t tcp_ticks = 0, state = 0, flags = 0, snd_buf = 0;
        uint32_t cwnd = 0, snd_wnd = 0, ssthresh = 0, lastack = 0;
        uint32_t snd_nxt = 0, snd_lbb = 0, rto_end = 0, bytes_acked = 0;
        uint32_t rttest = 0, rtseq = 0;
        uint16_t snd_queuelen = 0, mss = 0;
        int16_t rtime = 0, rto = 0, sa = 0, sv = 0;
        uint8_t nrtx = 0, dupacks = 0;
        Segments unacked, unsent;
    };
    struct Totals {
        uint32_t attempts = 0, queued = 0, queue_rejected = 0;
        uint32_t completed = 0, stale_discarded = 0, not_found = 0;
        uint32_t matched = 0, blocked_trigger = 0, truncated = 0;
        uint32_t snd_buf_zero = 0, snd_wnd_zero = 0, retransmitting_samples = 0;
        uint32_t min_snd_buf = 0, min_cwnd = 0, min_snd_wnd = 0;
        uint32_t max_unsent_bytes = 0, max_unacked_bytes = 0;
        uint8_t max_nrtx = 0, max_dupacks = 0;
        uint64_t dispatch_us_sum = 0, dispatch_us_max = 0;
        uint64_t callback_us_sum = 0, callback_us_max = 0;
    };
    struct Snapshot {
        bool initialized = false, message_ready = false, connected = false;
        bool tuple_valid = false, pending = false, observed = false;
        int tuple_error = 0, last_post_error = 0;
        uint64_t generation = 0, pending_generation = 0, pending_since_us = 0;
        Tuple tuple;
        Totals totals;
        Sample last;
    };

    RearTcpDiagnostic() = default;
    RearTcpDiagnostic(const RearTcpDiagnostic &) = delete;
    RearTcpDiagnostic &operator=(const RearTcpDiagnostic &) = delete;

    // The socket owner calls this on its first established WebSocket connection.
    // That proves lwIP initialization preceded the one allocation attempt.
    // Allocation failure disables diagnostics; it never prevents streaming.
    void begin() {
        if (owner.initialized) return;
        owner.initialized = true;
        message = tcpip_callbackmsg_new(callback, this);
        owner.message_ready = message != nullptr;
        publish();
    }

    // Socket-owner task only, once per WebSocket connection event. getname
    // performs a stack RPC: it is deliberately absent from service()/EAGAIN.
    void connection(bool online, int descriptor = -1) {
        ++owner.generation;
        owner.connected = online;
        owner.tuple_valid = false;
        owner.tuple_error = 0;
        owner.tuple = Tuple{};
        if (online) {
            sockaddr_in local{}, remote{};
            socklen_t localSize = sizeof(local), remoteSize = sizeof(remote);
            if (descriptor < 0) owner.tuple_error = ENOTCONN;
            else if (::getsockname(descriptor, reinterpret_cast<sockaddr *>(&local), &localSize) != 0)
                owner.tuple_error = errno;
            else if (::getpeername(descriptor, reinterpret_cast<sockaddr *>(&remote), &remoteSize) != 0)
                owner.tuple_error = errno;
            else if (localSize != sizeof(local) || remoteSize != sizeof(remote) ||
                     local.sin_family != AF_INET || remote.sin_family != AF_INET)
                owner.tuple_error = EAFNOSUPPORT;
            else {
                owner.tuple.localIp = local.sin_addr.s_addr;
                owner.tuple.remoteIp = remote.sin_addr.s_addr;
                owner.tuple.localPort = ntohs(local.sin_port);
                owner.tuple.remotePort = ntohs(remote.sin_port);
                owner.tuple_valid = true;
            }
        }
        // Request/result storage remains in callback custody across reconnect.
        publish();
    }

    // One socket-owner/requester. No allocation, socket API, PCB read or wait.
    // Called between loop iterations and after nonblocking sends. The only
    // queue operation is trycallback; failures are rate-limited as well.
    void service(bool wouldBlock) {
        if (phase.load(std::memory_order_acquire) == Ready) {
            const Sample done = result;
            owner.pending = false;
            ++owner.totals.completed;
            if (!owner.connected || done.generation != owner.generation) {
                ++owner.totals.stale_discarded;
            } else {
                owner.last = done;
                owner.observed = true;
                accumulate(done);
            }
            phase.store(Idle, std::memory_order_release);
            publish();
        }
        const uint64_t now = esp_timer_get_time();
        if (!owner.message_ready || !owner.connected || !owner.tuple_valid ||
            phase.load(std::memory_order_acquire) != Idle ||
            (attempted && now - lastAttemptUs < intervalUs)) return;
        attempted = true;
        lastAttemptUs = now;
        request.tuple = owner.tuple;
        request.generation = owner.generation;
        request.requestedUs = now;
        request.wouldBlock = wouldBlock;
        owner.pending = true;
        owner.pending_generation = request.generation;
        owner.pending_since_us = now;
        ++owner.totals.attempts;
        // All request bytes precede this release. The callback may execute and
        // publish Ready before trycallback returns. Never overwrite that Ready.
        phase.store(Pending, std::memory_order_release);
        publish();
        const err_t error = tcpip_callbackmsg_trycallback(message);
        // Submission can be descheduled. Space the next attempt from return,
        // while keeping requestedUs as the dispatch-latency measurement origin.
        lastAttemptUs = esp_timer_get_time();
        owner.last_post_error = error;
        if (error == ERR_OK) {
            ++owner.totals.queued;
        } else {
            ++owner.totals.queue_rejected;
            // The API guarantees no queued callback on error.
            phase.store(Idle, std::memory_order_release);
            owner.pending = false;
        }
        publish();
    }

    // HTTP copies only this cache under a short independent lock. The callback
    // never takes an application lock, and the HTTP path never queues a sample.
    Snapshot snapshot() {
        portENTER_CRITICAL(&cacheLock);
        const Snapshot copy = published;
        portEXIT_CRITICAL(&cacheLock);
        return copy;
    }

private:
    enum Phase : uint8_t { Idle, Pending, Ready };
    struct Request {
        Tuple tuple;
        uint64_t generation = 0, requestedUs = 0;
        bool wouldBlock = false;
    } request;
    std::atomic<uint8_t> phase{Idle};
    tcpip_callback_msg *message = nullptr;
    bool attempted = false;
    uint64_t lastAttemptUs = 0;
    Sample result; // Callback writes; owner reads only after acquire Ready.
    Snapshot owner, published;
    portMUX_TYPE cacheLock = portMUX_INITIALIZER_UNLOCKED;

    void publish() {
        portENTER_CRITICAL(&cacheLock);
        published = owner;
        portEXIT_CRITICAL(&cacheLock);
    }

    static void segments(const tcp_seg *node, Segments &out, unsigned &budget) {
        out.not_visited = node && !budget;
        while (node && budget) {
            const uint32_t start = lwip_ntohl(node->tcphdr->seqno);
            if (!out.visited) out.first_seq = start;
            out.last_end_seq = start + node->len;
            out.payload_bytes += node->len;
            ++out.visited;
            --budget;
            node = node->next;
        }
        out.truncated = node != nullptr;
    }

    static void callback(void *context) {
        auto *self = static_cast<RearTcpDiagnostic *>(context);
        if (self->phase.load(std::memory_order_acquire) != Pending) return;
        const Request req = self->request;
        Sample sample;
        sample.generation = req.generation;
        sample.requested_us = req.requestedUs;
        sample.would_block = req.wouldBlock;
        sample.started_us = esp_timer_get_time();
        sample.tcp_ticks = ::tcp_ticks;
        const tcp_pcb *pcb = tcp_active_pcbs;
        while (pcb && sample.pcbs_visited < maxPcbs) {
            ++sample.pcbs_visited;
            if (IP_IS_V4_VAL(pcb->local_ip) && IP_IS_V4_VAL(pcb->remote_ip) &&
                ip4_addr_get_u32(ip_2_ip4(&pcb->local_ip)) == req.tuple.localIp &&
                ip4_addr_get_u32(ip_2_ip4(&pcb->remote_ip)) == req.tuple.remoteIp &&
                pcb->local_port == req.tuple.localPort &&
                pcb->remote_port == req.tuple.remotePort) {
                sample.found = true;
#define REAR_TCP_COPY(field) sample.field = pcb->field
                REAR_TCP_COPY(state); REAR_TCP_COPY(flags); REAR_TCP_COPY(snd_buf);
                REAR_TCP_COPY(cwnd); REAR_TCP_COPY(snd_wnd); REAR_TCP_COPY(ssthresh);
                REAR_TCP_COPY(lastack); REAR_TCP_COPY(snd_nxt); REAR_TCP_COPY(snd_lbb);
                REAR_TCP_COPY(rto_end); REAR_TCP_COPY(bytes_acked);
                REAR_TCP_COPY(snd_queuelen); REAR_TCP_COPY(mss);
                REAR_TCP_COPY(rtime); REAR_TCP_COPY(rto); REAR_TCP_COPY(sa); REAR_TCP_COPY(sv);
                REAR_TCP_COPY(rttest); REAR_TCP_COPY(rtseq);
                REAR_TCP_COPY(nrtx); REAR_TCP_COPY(dupacks);
#undef REAR_TCP_COPY
                unsigned budget = maxSegments;
                segments(pcb->unacked, sample.unacked, budget);
                segments(pcb->unsent, sample.unsent, budget);
                break;
            }
            pcb = pcb->next;
        }
        sample.pcb_walk_truncated = !sample.found && pcb;
        sample.completed_us = esp_timer_get_time();
        self->result = sample; // Scalars only; never retain a PCB/segment pointer.
        self->phase.store(Ready, std::memory_order_release); // Last context access.
    }

    void accumulate(const Sample &s) {
        Totals &t = owner.totals;
        const uint64_t dispatch = s.started_us - s.requested_us;
        const uint64_t duration = s.completed_us - s.started_us;
        t.dispatch_us_sum += dispatch;
        if (dispatch > t.dispatch_us_max) t.dispatch_us_max = dispatch;
        t.callback_us_sum += duration;
        if (duration > t.callback_us_max) t.callback_us_max = duration;
        if (s.would_block) ++t.blocked_trigger;
        if (s.pcb_walk_truncated || s.unacked.truncated || s.unsent.truncated) ++t.truncated;
        if (!s.found) { ++t.not_found; return; }
        if (!t.matched || s.snd_buf < t.min_snd_buf) t.min_snd_buf = s.snd_buf;
        if (!t.matched || s.cwnd < t.min_cwnd) t.min_cwnd = s.cwnd;
        if (!t.matched || s.snd_wnd < t.min_snd_wnd) t.min_snd_wnd = s.snd_wnd;
        ++t.matched;
        if (!s.snd_buf) ++t.snd_buf_zero;
        if (!s.snd_wnd) ++t.snd_wnd_zero;
        if (s.nrtx) ++t.retransmitting_samples;
        if (s.nrtx > t.max_nrtx) t.max_nrtx = s.nrtx;
        if (s.dupacks > t.max_dupacks) t.max_dupacks = s.dupacks;
        if (s.unsent.payload_bytes > t.max_unsent_bytes) t.max_unsent_bytes = s.unsent.payload_bytes;
        if (s.unacked.payload_bytes > t.max_unacked_bytes) t.max_unacked_bytes = s.unacked.payload_bytes;
    }
};
