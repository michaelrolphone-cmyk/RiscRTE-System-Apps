#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>

ssize_t diagnosticSend(int, const void *, size_t, int);
#define send diagnosticSend
#define LIVE_WEBSOCKET_TCP_DIAGNOSTIC 1
#include "live_websocket.h"
#undef send

uint64_t transportTestClockUs = 1;
thread_local bool diagnosticInCallback = false;
tcp_pcb *tcp_active_pcbs = nullptr;
uint32_t tcp_ticks = 77;
struct tcpip_callback_msg { tcpip_callback_fn function; void *context; };
namespace {
tcpip_callback_msg pool[64];
unsigned allocations = 0, posts = 0, names = 0;
bool allocationFail = false, queueFail = false, immediate = false;
bool sending = false, wouldBlock = false;
uint16_t localPort = 40001;
uint32_t localIp = htonl(0xc0a8011f), remoteIp = htonl(0xc0a80137);
std::deque<tcpip_callback_msg *> mailbox;

void runOne() {
    assert(!mailbox.empty());
    auto *msg = mailbox.front(); mailbox.pop_front();
    assert(!diagnosticInCallback);
    diagnosticInCallback = true;
    msg->function(msg->context);
    diagnosticInCallback = false;
}
void reset() {
    assert(mailbox.empty());
    allocationFail = queueFail = immediate = sending = wouldBlock = false;
    posts = names = 0; localPort = 40001;
    transportTestClockUs = 1; tcp_active_pcbs = nullptr;
}
tcp_pcb matching() {
    tcp_pcb p;
    p.local_ip.addr = localIp; p.remote_ip.addr = remoteIp;
    p.local_port = localPort; p.remote_port = 9878;
    p.snd_buf = 0; p.cwnd = 2872; p.snd_wnd = 65535; p.ssthresh = 3000;
    p.lastack = 0xffffff00; p.snd_nxt = 100; p.snd_lbb = 200;
    p.rtime = -1; p.rto = 6; p.sa = 17; p.sv = 5; p.nrtx = 2; p.dupacks = 1;
    return p;
}
void connect(RearTcpDiagnostic &d) {
    d.begin(); d.connection(true, 123);
}
void drain(RearTcpDiagnostic &d) {
    d.connection(false);
    while (!mailbox.empty()) runOne();
    d.service(false);
}

void delayedReconnectCannotReuseOrRelabel() {
    reset(); RearTcpDiagnostic d; connect(d);
    tcp_pcb old = matching(); tcp_active_pcbs = &old;
    d.service(true);
    assert(posts == 1 && d.snapshot().pending && names == 2);
    const uint64_t oldGeneration = d.snapshot().generation;
    transportTestClockUs += 2000000;
    d.connection(false); localPort = 40002; d.connection(true, 124);
    for (int i = 0; i < 100; ++i) d.service(true);
    assert(posts == 1 && names == 4 && !d.snapshot().observed);
    assert(d.snapshot().pending_generation == oldGeneration);
    runOne(); // Old tuple may still exist, but its generation must be discarded.
    assert(d.snapshot().pending && !d.snapshot().observed);
    d.service(false); // Consume READY before the slot can be reused.
    assert(d.snapshot().totals.stale_discarded == 1 && !d.snapshot().observed);
    assert(posts == 2 && mailbox.size() == 1);
    tcp_pcb current = matching(); tcp_active_pcbs = &current;
    runOne(); d.service(false);
    auto s = d.snapshot();
    assert(s.observed && s.last.found && s.last.generation == s.generation);
    assert(!s.pending && s.totals.matched == 1 && names == 4);
    drain(d);
}

void callbackBeforePostReturnIsRetained() {
    reset(); immediate = true;
    RearTcpDiagnostic d; connect(d);
    tcp_pcb pcb = matching(); tcp_active_pcbs = &pcb;
    d.service(true); // API invokes the callback before returning ERR_OK.
    auto queued = d.snapshot();
    assert(queued.pending && queued.totals.queued == 1 && !queued.observed);
    d.service(false);
    auto s = d.snapshot();
    assert(s.observed && !s.pending && s.totals.completed == 1);
    assert(s.last.would_block && s.last.tcp_ticks == 77);
    assert(s.last.snd_buf == 0 && s.last.rtime == -1 && s.last.rto == 6);
    assert(s.last.sa == 17 && s.last.sv == 5 && s.last.mss == 1436);
    assert(s.last.completed_us - s.last.started_us == 3);
    assert(s.totals.snd_buf_zero == 1 && s.totals.retransmitting_samples == 1);
    drain(d);
}

void queueFullIsBoundedAndDoesNotHoldCustody() {
    reset(); RearTcpDiagnostic d; connect(d); queueFail = true;
    d.service(true);
    assert(!d.snapshot().pending && d.snapshot().totals.queue_rejected == 1);
    for (int i = 0; i < 1000; ++i) d.service(true);
    transportTestClockUs = 250000; d.service(true);
    assert(posts == 1);
    transportTestClockUs = 250001; queueFail = false; d.service(false);
    assert(posts == 2 && d.snapshot().pending);
    transportTestClockUs += 42000;
    runOne(); d.service(false);
    auto s = d.snapshot();
    assert(s.observed && !s.last.found && s.totals.not_found == 1);
    assert(s.last.started_us - s.last.requested_us == 42000);
    assert(s.totals.dispatch_us_max == 42000 && s.totals.matched == 0);
    drain(d);
}

void pcbAndSegmentWorkAreCapped() {
    reset(); RearTcpDiagnostic d; connect(d);
    std::vector<tcp_pcb> pcbs(33);
    for (size_t i=0;i+1<pcbs.size();++i) pcbs[i].next=&pcbs[i+1];
    pcbs.back() = matching(); tcp_active_pcbs=pcbs.data();
    d.service(false); runOne(); d.service(false);
    auto s=d.snapshot();
    assert(!s.last.found && s.last.pcbs_visited==32 && s.last.pcb_walk_truncated);
    transportTestClockUs += 250000;
    tcp_pcb pcb=matching();
    tcp_hdr headers[18]; tcp_seg segments[18];
    for(unsigned i=0;i<18;++i) {
        headers[i].seqno=htonl(1000+i*100);
        segments[i].tcphdr=&headers[i]; segments[i].len=100;
        if(i<16) segments[i].next=&segments[i+1];
    }
    pcb.unacked=&segments[0]; pcb.unsent=&segments[17]; tcp_active_pcbs=&pcb;
    d.service(true); runOne(); d.service(false); s=d.snapshot();
    assert(s.last.found && s.last.unacked.visited==16);
    assert(s.last.unacked.payload_bytes==1600 && s.last.unacked.truncated);
    assert(s.last.unsent.not_visited && s.last.unsent.truncated && !s.last.unsent.visited);
    assert(s.last.unacked.first_seq==1000 && s.last.unacked.last_end_seq==2600);
    // A preserved cache value is a copy, unaffected by later PCB mutations.
    pcb.snd_buf=5760;
    assert(s.last.snd_buf==0 && d.snapshot().last.snd_buf==0);
    drain(d);
}

void ipv4TupleAndZeroValuesAreNotGuessed() {
    reset(); RearTcpDiagnostic d; connect(d);
    tcp_pcb wrong=matching(), correct=matching();
    wrong.remote_ip.type=6; wrong.next=&correct; correct.snd_wnd=0; correct.nrtx=0;
    tcp_active_pcbs=&wrong;
    d.service(false); runOne(); d.service(false);
    const auto s=d.snapshot();
    assert(s.last.found && s.last.pcbs_visited==2 && s.last.snd_wnd==0);
    assert(s.totals.snd_wnd_zero==1 && s.last.nrtx==0);
    d.connection(false);
    assert(d.snapshot().observed && d.snapshot().last.generation != d.snapshot().generation);
    drain(d);
}

void clientsHaveIndependentPersistentCustody() {
    reset(); RearTcpDiagnostic first, second;
    connect(first);
    const uint16_t firstPort=localPort;
    localPort=40003; connect(second);
    tcp_pcb firstPcb=matching(), secondPcb=matching();
    firstPcb.local_port=firstPort; firstPcb.next=&secondPcb;
    tcp_active_pcbs=&firstPcb;
    first.service(true); second.service(false);
    assert(mailbox.size()==2 && posts==2);
    first.connection(false);
    runOne(); first.service(false);
    assert(first.snapshot().totals.stale_discarded==1);
    assert(second.snapshot().pending && !second.snapshot().observed);
    runOne(); second.service(false);
    assert(second.snapshot().observed && second.snapshot().last.found);
    assert(!second.snapshot().last.would_block && second.snapshot().totals.matched==1);
    drain(first); drain(second);
}

void allocationFailureLeavesWriterFunctional() {
    reset(); allocationFail=true;
    RearTcpDiagnostic d;
    const unsigned before=allocations;
    TestTcp tcp(open("/dev/null", O_WRONLY)); LiveWebSocket socket; socket.attach(tcp);
    socket.attachTcpDiagnostic(d); socket.diagnosticTransportChanged(true);
    socket.diagnosticTransportChanged(false); socket.diagnosticTransportChanged(true);
    assert(allocations==before+1 && !d.snapshot().message_ready);
    uint8_t payload[WEBSOCKETS_MAX_HEADER_SIZE+32]={};
    sending=true;
    assert(socket.writeLive(WSop_binary,payload,32,false));
    sending=false;
    assert(posts==0 && d.snapshot().totals.attempts==0 && socket.disconnects==0);
    drain(d);
}

void eagainDoesNotAllocateOrQuerySocket() {
    reset(); RearTcpDiagnostic d;
    TestTcp tcp(open("/dev/null", O_WRONLY)); LiveWebSocket socket; socket.attach(tcp);
    socket.attachTcpDiagnostic(d); socket.diagnosticTransportChanged(true);
    tcp_pcb pcb=matching(); tcp_active_pcbs=&pcb;
    const unsigned before=allocations, getNames=names;
    wouldBlock=true; immediate=true; sending=true;
    uint8_t payload[WEBSOCKETS_MAX_HEADER_SIZE+32]={};
    assert(!socket.writeLive(WSop_binary,payload,32,false));
    sending=false;
    assert(socket.writeTimeoutCount()==1 && socket.disconnects==1);
    assert(allocations==before && names==getNames);
    // Four-second progress timeout is unchanged; samples are <=4 per second.
    assert(posts<=17 && posts>=16);
    assert(d.snapshot().totals.blocked_trigger>=15);
    socket.diagnosticTransportChanged(false);
    drain(d);
}
}

tcpip_callback_msg *tcpip_callbackmsg_new(tcpip_callback_fn fn, void *ctx) {
    assert(!diagnosticInCallback && !sending);
    ++allocations;
    if(allocationFail) return nullptr;
    assert(allocations<64);
    pool[allocations].function=fn; pool[allocations].context=ctx;
    return &pool[allocations];
}
err_t tcpip_callbackmsg_trycallback(tcpip_callback_msg *msg) {
    assert(!diagnosticInCallback);
    ++posts;
    if(queueFail) return ERR_MEM;
    mailbox.push_back(msg);
    if(immediate) runOne();
    return ERR_OK;
}
int diagnosticGetSockName(int, sockaddr *address, socklen_t *size) {
    assert(!diagnosticInCallback && !sending); ++names;
    auto *out=reinterpret_cast<sockaddr_in *>(address);
    std::memset(out,0,sizeof(*out)); out->sin_family=AF_INET;
    out->sin_addr.s_addr=localIp; out->sin_port=htons(localPort); *size=sizeof(*out);
    return 0;
}
int diagnosticGetPeerName(int, sockaddr *address, socklen_t *size) {
    assert(!diagnosticInCallback && !sending); ++names;
    auto *out=reinterpret_cast<sockaddr_in *>(address);
    std::memset(out,0,sizeof(*out)); out->sin_family=AF_INET;
    out->sin_addr.s_addr=remoteIp; out->sin_port=htons(9878); *size=sizeof(*out);
    return 0;
}
ssize_t diagnosticSend(int, const void *, size_t size, int flags) {
    assert(flags==MSG_DONTWAIT);
    if(wouldBlock){errno=EAGAIN;return -1;}
    return static_cast<ssize_t>(size);
}

int main() {
    static_assert(LiveWriteProgress::idleLimitUs==4000000,"Materialized rear budget required");
    delayedReconnectCannotReuseOrRelabel(); puts("PASS delayed callback and reconnect custody");
    callbackBeforePostReturnIsRetained(); puts("PASS callback completes before post returns");
    queueFullIsBoundedAndDoesNotHoldCustody(); puts("PASS queue full, throttle and measured dispatch delay");
    pcbAndSegmentWorkAreCapped(); puts("PASS bounded PCB/segment walk and scalar-only cache");
    ipv4TupleAndZeroValuesAreNotGuessed(); puts("PASS exact IPv4 tuple, zero values and stale generation");
    clientsHaveIndependentPersistentCustody(); puts("PASS independent persistent client requests");
    allocationFailureLeavesWriterFunctional(); puts("PASS one allocation failure leaves sending functional");
    eagainDoesNotAllocateOrQuerySocket(); puts("PASS real writer EAGAIN has no allocation/getters and retains deadline");
    puts("8 production TCP diagnostic groups passed");
}
