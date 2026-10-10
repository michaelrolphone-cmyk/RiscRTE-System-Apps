#pragma once
/* App-facing, explicitly bound nonblocking IPv4 TCP listener. The ordinary
 * provider owns the native transport dependency. No app-data grants, filesystem
 * authority, credentials, radio control or network startup cross this ABI. */
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define RISC_TCP_CONNECTION_CAPABILITY "network.tcp.listener"
#define RISC_TCP_CONNECTION_API_V1 1u
#define RISC_TCP_CONNECTION_LISTENERS_MAX 1u
#define RISC_TCP_CONNECTION_CLIENTS_MAX 4u
#define RISC_TCP_CONNECTION_BYTES_MAX 2048u
enum {
    RISC_TCP_CONNECTION_OK = 0,
    RISC_TCP_CONNECTION_WOULD_BLOCK = 1,
    RISC_TCP_CONNECTION_EOF = 2,
    RISC_TCP_CONNECTION_INVALID = -1,
    RISC_TCP_CONNECTION_CONTEXT = -2,
    RISC_TCP_CONNECTION_LIMIT = -3,
    RISC_TCP_CONNECTION_NETWORK_DOWN = -4,
    RISC_TCP_CONNECTION_IO = -5,
    RISC_TCP_CONNECTION_RETAINED = -6,
    RISC_TCP_CONNECTION_BUSY = -7
};
typedef struct {
    uint32_t struct_size;
    uint8_t address[4]; /* Network-order IPv4 octets; 0.0.0.0 explicitly binds any. */
    uint16_t port;      /* Host byte order, 1..65535. No automatic/ephemeral port. */
    uint16_t reserved;  /* Must be zero. */
} risc_tcp_connection_listen_v1;
typedef struct {
    uint32_t api_version, struct_size;
    void *context; /* Opaque activation token; copy unchanged, never dereference. */
    int32_t (*listen)(void *, const risc_tcp_connection_listen_v1 *, uint64_t *);
    int32_t (*accept)(void *, uint64_t, uint64_t *);
    /* One bounded operation per call. Buffers are copied through provider-owned
     * memory and are not retained. OK reports 1..capacity bytes; partial writes
     * are normal. WOULD_BLOCK and EOF report zero. EOF is read-only. Call again
     * on a later cooperative tick after WOULD_BLOCK; never spin in the caller. */
    int32_t (*read)(void *, uint64_t, void *, uint32_t, uint32_t *);
    int32_t (*write)(void *, uint64_t, const void *, uint32_t, uint32_t *);
    /* Clients must close before their listener (otherwise BUSY). Successful
     * close permanently retires that handle. CONTEXT/RETAINED from the native
     * dependency fence the entire activation; ambiguous close is never retried
     * and quiesce refuses unload. Local stale handles return CONTEXT without
     * calling the native dependency. Retained resources require native recovery.
     * Copies of this table/callbacks are valid only while their grant is live. */
    int32_t (*close)(void *, uint64_t);
} risc_tcp_connection_v1;
#ifdef __cplusplus
}
#endif
