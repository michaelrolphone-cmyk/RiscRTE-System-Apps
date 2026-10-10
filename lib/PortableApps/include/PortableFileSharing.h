#pragma once
#include "RiscRuntimeV1.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* App-local controller, not an ABI exported by Runtime. The caller explicitly
 * starts each finite sharing session and supplies the configured grant IDs.
 * No network startup, persistent credentials or filesystem roots are inferred.
 * All callbacks run on the owning foreground task. owner is a pure cached
 * predicate and must not call a capability or native diagnostic service. */
typedef struct portable_file_sharing portable_file_sharing;
typedef struct {
 void *context;
 bool (*owner)(void *);
 void *(*allocate)(size_t);
 void (*deallocate)(void *);
} portable_file_sharing_hooks;
typedef struct {
 uint64_t files_instance,tcp_instance,entropy_instance;
 uint8_t address[4]; /* Explicit bind; zero means all local IPv4 addresses. */
 uint16_t port;
 uint32_t lifetime_ms; /* 1..3600000; session begins at explicit start. */
 bool authenticated_http; /* User selected authenticated, unencrypted HTTP. */
} portable_file_sharing_config;
enum {
 PORTABLE_FILE_SHARING_OFF,PORTABLE_FILE_SHARING_PREPARING,
 PORTABLE_FILE_SHARING_LISTENING,PORTABLE_FILE_SHARING_SERVING,
 PORTABLE_FILE_SHARING_STOPPING,PORTABLE_FILE_SHARING_ERROR,
 PORTABLE_FILE_SHARING_RETAINED
};
typedef struct {
 unsigned state;
 int32_t error;
 unsigned last_http_status;
 uint32_t completed_requests;
 uint64_t expires_ms;
 uint16_t port;
 bool active_work;
 char username[6];
 char password[17]; /* Ephemeral display copy; empty before ready/after stop. */
} portable_file_sharing_status;
/* create performs only app-memory allocation. A failed allocation performs no
 * capability I/O. start also performs no I/O; tick acquires/generates/listens in
 * separate bounded steps. Ordinary tick performs at most one capability call. */
portable_file_sharing *portable_file_sharing_create(const risc_runtime_api_v1 *,const portable_file_sharing_hooks *);
bool portable_file_sharing_start(portable_file_sharing *,const portable_file_sharing_config *,uint64_t now);
void portable_file_sharing_tick(portable_file_sharing *,uint64_t now);
void portable_file_sharing_stop(portable_file_sharing *);
void portable_file_sharing_get_status(const portable_file_sharing *,portable_file_sharing_status *);
/* Manual transitions may drain the bounded nonblocking close sequence. Returns
 * false on terminal custody, never retries an ambiguous close/release. The app
 * must retain its invocation then. No I/O follows RETAINED or owner loss. */
bool portable_file_sharing_close(portable_file_sharing *,uint64_t now);
/* Only destroys a clean OFF/ERROR controller; returns false if still owned.
 * On success sets the caller pointer to NULL. */
bool portable_file_sharing_destroy(portable_file_sharing **);
#ifdef __cplusplus
}
#endif
