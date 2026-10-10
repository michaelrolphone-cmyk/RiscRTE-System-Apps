# Foreground WebDAV saved-file protocol

This source continues the recovered shared browser and actual app-data export.
It adds no product listener, credentials, pairing, display or background task.
It is not a reconstruction of the lost .102 source. The Reader repository is
unchanged; its navigation/association lineage remains in the existing browser.
This protocol uses the explicitly configured logical export catalog and the
existing atomic AppData backend. It does not enumerate bootfs, namespaces, NVS,
private staging or unconfigured files.

`Transaction` processes OPTIONS, GET, HEAD, PROPFIND and PUT with a required
caller authorization hook. An overwrite requires a specific strong If-Match
revision. Creation requires If-None-Match: *. A conflicting owner write produces
412; commit uncertainty produces a reload error and never replays a mutation.
Unknown paths are rejected before storage access. Read-only entries remain
read-only. GET snapshots use matching stat/read revision tokens. Configured
missing files can be created, and do not appear as existing PROPFIND children.
Unsupported directory mutations, deletion, COPY/MOVE and locks return 405. No
DAV compliance class is advertised for this deliberately limited export.

PROPFIND supports empty body/allprop, propname, named properties and include,
with expanded XML namespace names. Unknown properties return 404 propstat.
Only actual metadata is emitted; creation/last-modified dates are not invented.
The parser rejects DTDs and custom entities, accepts bounded UTF-8 XML 1.0, and
retains no input pointers. Depth 0 and 1 are supported; omitted/infinity for a
collection returns 403. A property response is bounded at 16 KiB.

`Session` owns one finite HTTP/1.1 request and an already accepted connection
from an ordinary admitted transport facade. It preserves partial reads/writes,
handles fragmented headers and bodies, sends authorized 100 Continue, and
closes each connection after one response. Header/body limits are 4096/65536
bytes; one tick performs at most one transport or storage operation. The
foreground owner supplies a monotonic deadline and retains its file/transport
grants until checked completion. The explicit Headers auth phase runs before body allocation; Complete receives
the final body before any export-catalog access. Only Complete may consume
a Digest nonce count or approve body integrity. The original request-target is
kept separately from its checked logical path for standard Digest calculation. This hook is not an authentication
implementation: credentials, encryption, session controls and BLE brokering
must be selected and qualified before a product exposes an endpoint.

Cancellation closes the socket before releasing owned memory. Context loss or
RETAINED from either storage or transport prohibits response, socket cleanup,
file-grant release and memory cleanup. An ambiguous close is never retried.
A live network connection must also inhibit automatic sleep through the shared
foreground policy; manual sleep requires checked transport shutdown. Neither
behavior is activated by compiling these library sources.

The objects contain bounded request/catalog/parser state and should live in the
foreground app's allocated storage rather than a small native call stack.
The largest request body or file snapshot is 64 KiB; the existing AppData backend
may allocate its own complete-file snapshot during an operation. Final product
heap and flash-store budgets remain to be measured with all selected apps.

## Verification

- `bash scripts/test_webdav_core.sh`
- `ASAN_OPTIONS=detect_leaks=0 SANITIZE=1 bash scripts/test_webdav_core.sh`
- `python3 scripts/test_webdav_properties.py`
- `bash scripts/test_webdav_appdata.sh MATCHING_RUNTIME`
- `ASAN_OPTIONS=detect_leaks=0 SANITIZE=1 bash scripts/test_webdav_appdata.sh MATCHING_RUNTIME`

`scripts/build_webdav_protocol_probe.py` also compiles the actual three modules
with Xtensa GCC 8.4 and checks their complete undefined-symbol set against the
existing bounded libc operations. This is an object-level target compile, not a
packaged application or whole-store admission.

The core/session tests compile the actual protocol source. They cover strict
HTTP framing, auth-before-storage, scope, weak/strong ETags, conditional writes,
owner races, allocation faults, XML property selection, fragmented input,
partial output, would-block, EOF, deadlines, cancellation and terminal fences.
The XML parser has 16,404 deterministic assertions in each mode. The final eight
integration cases use production AppDataFiles and AppDataExport with real host
filesystem operations, including owner-write conflict and checked-close failure.
Installed and private-file sentinels remain unchanged. ASan/UBSan pass; only
LeakSanitizer is disabled because this executor uses ptrace. No network socket,
physical device, deployment or TLS interoperability was exercised by these tests.

References: [WebDAV RFC4918](https://www.rfc-editor.org/rfc/rfc4918.html),
[HTTP semantics RFC9110](https://www.rfc-editor.org/rfc/rfc9110.html),
[XML Namespaces](https://www.w3.org/TR/xml-names/).

## Explicit authenticated sharing controller

`DigestSession` implements RFC 7616 SHA-256 with auth/auth-int, exact raw URI and
method binding, a finite caller-selected nonce lifetime and bounded replay
tracking. It requires fresh cryptographic nonce bytes. Digest does not encrypt
file contents; the Files UI must clearly identify HTTP versus optional HTTPS.
No endpoint is enabled at boot by this library.

`Sharing` accepts already admitted export/transport grants and initialized
Digest credentials. Start is explicit and state-only; foreground ticks open one
listener, handle one request at a time, and close each client before accepting
the next. Caller cancellation and expiry drain client then listener. Every tick
also checks Digest liveness, so a shorter authentication expiry or explicit
clear prevents any staged file write and closes the listener. Checked closure
must finish before manual sleep, radio changes or app/grant release. Automatic
idle sleep must be inhibited throughout the explicitly active sharing session.

`bash scripts/test_webdav_sharing.sh` compiles actual Digest, Core, Session,
Sharing and the TCP provider. Its ten cases cover GET, conditional auth-int PUT,
unauthorized requests, changed bodies, stop, both expiry lifetimes, explicit
credential clearing, socket-close failure and storage retention. Normal and
ASan/UBSan runs pass. The authentication-expiry write regression failed before
the liveness correction. The target probe now compiles all five modules and the
BSD-licensed SHA-256 source together. These tests do not qualify app UI,
BLE setup, live client interoperability or hardware network/sleep timing.

An independent review completed the earlier protocol scope and identified the
Sharing expiry defect above. Its remaining Sharing/Digest review was interrupted
by a generic service flag with no specific action identified; that portion is
incomplete and has not been repeated.
