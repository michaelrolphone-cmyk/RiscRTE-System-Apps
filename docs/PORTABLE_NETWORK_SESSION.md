# Foreground saved-network sessions

`PortableNetworkSession.h` / `src/PortableNetworkSession.c` provide an application-local
session for Files/WebDAV and other foreground network clients. The caller supplies
already-authorized `net.wifi@1` instance 15 and `storage.key-value@1` namespace 6
tables, retains their grants, and supplies a pure cached owner/custody predicate.
The helper neither acquires nor releases grants and has no Runtime exports.

The explicit network-tool default is profile slot zero. The helper uses
`portable_wifi_profile_load` from `PortableWifiProfiles.h` with the existing selector
and chunk codec; it does not enumerate profiles, inspect unselected chunks, write
credentials, migrate data, or infer a last-used network. A healthy existing station
with a nonzero IPv4 address is borrowed immediately, even if slot zero is absent.
A joining station is allowed to finish without disruption. A borrowed connection
that disappears reports failure and is never automatically reconnected.

## Frontend integration

1. Initialize fresh session storage with `portable_network_session_init(session,
   wifi, kv, owner_ok, owner_context)`. Initialization has no backend calls. Never
   initialize over live or retained custody.
2. On the user's explicit Share action, call `portable_network_session_start`.
   Start only arms work. Call `portable_network_session_step(session, millis)` once
   per foreground tick. Idle or completed sessions can be started again explicitly.
3. `PORTABLE_NETWORK_PENDING` is ordinary progress. `PORTABLE_NETWORK_READY` means
   a checked copied link/IP snapshot is available. EMPTY/INVALID describe the saved
   profile; UNAVAILABLE describes unavailable tables/storage/provider startup;
   FAILED describes terminal link/timeout failure. RETAINED is a permanent custody
   fence. `portable_network_session_snapshot` copies state without backend I/O.
4. Pair `portable_network_session_service_begin` / `service_end` around bounded
   WebDAV/storage service work. Start no work on SERVICE_PENDING. Nested calls
   share one native lease. If end returns SERVICE_PENDING, retry end later without
   repeating the protected work; its depth/token remain owned until success.
5. Stop the WebDAV listener and clients, finish service lease scopes, and then call
   `portable_network_session_close`. CLOSE_PENDING means cancel/terminal polling
   is still healthy: keep processing bounded foreground ticks and defer manual
   sleep or launch. It is not a reason to retain the application. CLOSED permits
   the caller's own checked grant release and subsequent transition.
6. CLOSE_RETAINED, SERVICE_RETAINED, or result RETAINED requires retaining the
   invocation, tables, operation, lease, and caller's grants. Do no further normal
   provider I/O. The helper makes no backend calls after its fence latches.

A close cancels only an operation ID accepted by this session. Borrowed connections
are left running. This does not transfer ownership of caller-supplied tables or
permit release of another component's grants. Releasing a grant is outside the
helper's contract and remains the caller's responsibility.

## Bounds and custody

Each foreground step makes at most four profile-codec reads, three copied station
reads, or one asynchronous operation callback. There is no backend polling loop,
thread, sleep, implicit retry scheduler, or synchronous-connect fallback. The complete
Wi-Fi asynchronous suffix, tag, version, and callbacks are required. Provider startup
UNAVAILABLE remains unavailable even when the old synchronous prefix exists.

Every backend callback, including each individual codec read, is fenced immediately
before and after by the cached owner predicate. Storage CONTEXT, owner loss, RETAINED,
malformed progress, contradictory accepted IDs, and ambiguous lease/cleanup returns
latch retention. Any ID returned at admission is kept before the post-call owner
check. An INVALID async admission is retained because the helper already constructed
a validated request; that response cannot establish a valid provider context.

BUSY/AGAIN with no accepted ID defers without marking failed cleanup. Accepted
requests are copied and credentials are volatile-wiped as soon as admission returns;
retryable admission retains only the still-needed private request until retry, cancel,
or the 30-second foreground join timeout. Codec scratch credentials are wiped after
every read. Closing also wipes any unsubmitted request promptly.

Cancellation has no completion deadline. Even cancel's QUIESCENT response is followed
on another tick by a validated terminal progress snapshot before dropping custody.
The join timeout requests cancellation; it never establishes quiescence by itself.
Close waits for explicit service_end scopes instead of silently releasing a caller's
active protected work.

## Qualification

Run the actual helper plus actual profile codec against bounded synthetic tables:

```sh
python3 scripts/test_portable_network_session.py
```

This runs normal and AddressSanitizer/UndefinedBehaviorSanitizer builds. It covers
slot-zero selection, profile absence, owner loss at every profile read, CONTEXT and
BUSY reads, stale/malformed async records, copied request/status/IP ownership,
borrowed links, joining/no-IP links, transient admission/poll/cancel, startup
UNAVAILABLE without sync fallback, timeout cancellation, pending versus retained
cleanup, nested leases, close waiting for leases, and failed lease cleanup.

Run the production provider seam in both modes:

```sh
python3 scripts/test_network_session_provider.py \
  --runtime /path/to/Runtime --watch /path/to/Watch \
  --output /path/to/normal-proof
python3 scripts/test_network_session_provider.py \
  --runtime /path/to/Runtime --watch /path/to/Watch \
  --output /path/to/sanitized-proof --sanitize
```

The seam links this helper and codec to the actual Watch Wi-Fi provider, CpuPort,
NativeRadioAsync worker, and NativeRadio lifecycle. It reuses Runtime's deterministic
SDK concurrency fixture, without modifying Runtime or Watch sources. Cases exercise
blocked-worker close with continuing foreground progress, startup unavailability,
accepted saved join with copied credentials/IP and service exclusion, borrowing an
already-connected external operation without cancellation, and retained native
cleanup failure. Commands, per-case logs, source hashes and evidence are written to
the output directory. Outer KV/owner and SDK boundaries remain synthetic; this is
not physical RF/NVS, end-to-end Files UI, or device qualification.

In a ptrace executor LeakSanitizer cannot run; explicitly use
`ASAN_OPTIONS=detect_leaks=0` there. AddressSanitizer and UndefinedBehaviorSanitizer
remain enabled. The runner otherwise preserves the caller's sanitizer policy.
