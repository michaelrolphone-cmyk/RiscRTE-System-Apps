# TCP listener facade

Development-only ordinary provider `network-tcp-listener`, exposing
`network.tcp.listener@1` through `RiscTcpConnectionV1.h`. Its sole dependency is
the provider-only `platform.tcp-listener@1`. The checked-in native ABI is copied
unchanged from runtime revision `96e8c1f4e76ba3accfdff33d81b06233ecc5e7f5`,
`sdk/driver/RiscTcpListenerV1.h`, SHA-256
`b748d83d6d6e3f143a36b26993b6b9495ddd11e14eeb083325c1e0ff4563fcc5`.

Start validates and copies the native table, without opening any socket or
starting Wi-Fi. A caller must explicitly supply IPv4 bind octets and a nonzero
port to `listen`. There is one listener and at most four clients. Each transfer
copies through a 2048-byte provider-owned buffer, returns after one native call,
and preserves partial success, WOULD_BLOCK, read-only EOF, and retained/context
loss distinctions. No caller buffer is retained or passed through to native I/O.
No app-data grant, path, filesystem operation, or protocol parser is accepted.

Activation tokens and facade session handles increase monotonically within the
mapped provider and refuse overflow. Released handles and prior activation
tokens fail before native I/O. As with all provider callbacks, copied tables are
valid only during their live runtime grant; they must not survive ELF unload.

Quiesce closes held clients before the listener. A failed/ambiguous native close,
native CONTEXT or RETAINED, or malformed native success latches a permanent
fence. Further operations cannot call native I/O; quiesce stays false and stop
cannot erase custody. The runtime must keep this ELF and its dependencies
pinned. NETWORK_DOWN and ordinary I/O failures leave cleanup available. There
are no automatic retries of a close that may already have happened.

## Development checks

Run `bash scripts/test_tcp_listener_service.sh`. The test compiles the actual
service with a fake native backend and exercises the descriptor's public entry,
start, callbacks, quiesce, stop and restart. The only direct state manipulation
places monotonic counters at their overflow boundary. For sanitizers, set
`SANITIZE=1`; environments using ptrace may also need
`ASAN_OPTIONS=detect_leaks=0` because LeakSanitizer cannot run under ptrace.

Run `python scripts/build_tcp_listener_service.py` with `NATIVE_APP_CC` pointing
to an installed ESP32-S3 GCC, or configure `PLATFORMIO_CORE_DIR`. The script
checks the pinned ABI, sole dependency, bounded imports/exports, ELF structure,
bounded BSS, absence of global constructors, and per-function stack usage. It
records input/output hashes without activating, publishing, or changing any
product version. Host tests and ELF validation do not qualify device timing,
Wi-Fi custody, network exposure policy, or actual socket behavior on hardware.
