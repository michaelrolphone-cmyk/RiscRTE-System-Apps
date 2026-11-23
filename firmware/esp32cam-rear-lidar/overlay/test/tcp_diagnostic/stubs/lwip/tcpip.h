#pragma once
#include <cstdint>
using err_t = int8_t;
constexpr err_t ERR_OK = 0, ERR_MEM = -1;
using tcpip_callback_fn = void (*)(void *);
struct tcpip_callback_msg;
tcpip_callback_msg *tcpip_callbackmsg_new(tcpip_callback_fn, void *);
err_t tcpip_callbackmsg_trycallback(tcpip_callback_msg *);
