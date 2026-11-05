# Nearby Files alarm-service lease regression

A private Nearby JOIN owns `portable_peer_files::network`, while the shared
Files service hooks previously checked only the server's `fsh_network`. The
server network is uninitialized in Nearby mode. Consequently the shared alarm
adapter's correctly placed service scope returned success without acquiring a
native lease. If the native worker then owned storage resources, the alarm
provider received a transient bound-storage BUSY and exposed `ALARM_STORAGE`.
The modal renders that state as SERVICE ERROR. No RTC call is needed.

The host reproduction uses the actual Files shared adapter, controller, peer,
network and WebDAV implementations and the production Utilities catalog alarm
profile (matching the delivered 0.4.8 profile). Only hardware and transport
boundaries are doubles. Both RGB and paper reproduce BLOCKED/-4 on the original
System commit after a private JOIN, with one storage read and zero RTC reads.
This establishes the code defect and a cause consistent with the hardware log;
the hardware log itself does not record the provider's error code.

The fix routes foreground service scopes through the peer's own network lease.
BUSY defers the alarm work. Pending lease-end drains without replay. Retained
results preserve terminal custody. Peer tick and close remain outside external
service scopes, so JOIN/poll/cancel can progress; peer TCP/storage work continues
using its existing internal scopes. The 1.5.30 resident ownership gate is intact.

Run with sibling `runtime` and `utilities` checkouts (or set `RUNTIME_ROOT` and
`UTILITIES_ROOT`):

```
bash scripts/test_file_browser_peer_alarm.sh
SANITIZE=1 bash scripts/test_file_browser_peer_alarm.sh
```

`SCENARIO=reproduce` is intended only for the original production source plus
this test seam. The fixed default covers JOIN BUSY, connected BUSY, borrowed LAN,
pending end without replay, and retained begin/end on RGB and paper. Successful
cases continue through authenticated root-directory listing and clean teardown.

The receipt records pinned inputs and source hashes. No hardware qualification,
release, version bump or alarm-provider modification is claimed here.
