#pragma once
#include <cstdint>
#include <arpa/inet.h>
#include "lwip/opt.h"
#define TCP_SLOW_INTERVAL 500
struct ip_addr_t { uint32_t addr = 0; uint8_t type = 0; };
#define IP_IS_V4_VAL(ip) ((ip).type == 0)
#define ip_2_ip4(ip) (ip)
#define ip4_addr_get_u32(ip) ((ip)->addr)
#define lwip_ntohl(v) ntohl(v)
struct tcp_hdr { uint32_t seqno = 0; };
struct tcp_seg { tcp_seg *next = nullptr; uint16_t len = 0; tcp_hdr *tcphdr = nullptr; };
// Boundary fake for the reviewed ABI. The target build additionally compiles
// against hash-guarded real headers/archive, not these host declarations.
struct tcp_pcb {
    tcp_pcb *next = nullptr;
    ip_addr_t local_ip, remote_ip;
    uint16_t local_port = 0, remote_port = 0;
    uint32_t state = 4, flags = 0, snd_buf = 0, cwnd = 0, snd_wnd = 0, ssthresh = 0;
    uint32_t lastack = 0, snd_nxt = 0, snd_lbb = 0, rto_end = 0, bytes_acked = 0;
    uint32_t rttest = 0, rtseq = 0;
    uint16_t snd_queuelen = 0, mss = 1436;
    int16_t rtime = 0, rto = 0, sa = 0, sv = 0;
    uint8_t nrtx = 0, dupacks = 0;
    tcp_seg *unacked = nullptr, *unsent = nullptr;
};
extern tcp_pcb *tcp_active_pcbs;
extern uint32_t tcp_ticks;
