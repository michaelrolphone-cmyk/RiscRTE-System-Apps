# Native Serial Monitor UI: isolated local candidate

This is a **receive-only, unmanifested source candidate**, based on System
`f9d55dbeb941ba40fb965f53172ce3e1b85af376`. It is not a delivered System utility,
product selection or release. The System repository's existing scope places
MCU/debug utilities outside the foundational application inventory. This local
branch keeps the controller beside its qualified client for qualification only;
its eventual utility repository and product launch composition are not chosen.

## Implemented behavior

- Explicit nonzero provider-instance choices, supplied by a bounded linked
  composition catalog (one to eight unique entries). No default instance,
  registry-order fallback, automatic selection or device scanning.
- Explicit endpoint selection from a complete provider snapshot. The displayed
  ordinal and provider-reported transport are the only available descriptive
  metadata. Opening uses the exact selected device and generation, without
  refresh/rebinding. A failed refresh invalidates UI activation, even though the
  client's prior snapshot remains retained for custody.
- Baud selection from 9600 through 921600, fixed 8N1/no flow control. Default UI
  selection is 115200, not an automatic connection. No TX, line editor, DTR/RTS
  control, flashing, role switching or serial protocol inference is implemented.
- Receive-only terminal with 64 bounded lines of up to 80 display columns,
  wrapping to the current geometry. Case and ASCII punctuation are preserved.
  CR returns the cursor; LF advances the line; Backspace erases the preceding
  character; Tab expands to a four-column boundary. Other control and non-ASCII
  bytes display as `[HH]`. ANSI escapes and UTF-8 are not interpreted. No data is
  written to storage or exported.
- Touch rows and Prev/Select/Next footer work without button-navigation
  composition. Older/Live/Newer navigates terminal history. Button events are
  also supported when a separately admitted input provider supplies them.
  Neutral input is required between actions, preventing held/repeated activation
  across page changes. Clear affects only display history, not RX byte counts.
- Disconnect/error is visible without reopening. Back closes before returning to
  instance selection. In this isolated profile, physical Home follows in-app
  Back rather than an unconfigured product route. Host exit and input failure
  close and return from the invocation; the controller never queues another
  application. Successful close permits a fresh
  explicit open. No provider or UI work follows retained custody.

The actual UI uses an app-local fixed-cell ASCII raster, not the adapter's compact
uppercase-only label font. `scripts/generate_serial_monitor_font.py` deterministically
rasterizes the already-vendored Rajdhani-600 font to 95 × 12 bytes. Its input SHA-256
is pinned in the script; the original copyright and SIL OFL 1.1 are in
`lib/PortableApps/paper_fonts/LICENSE-Rajdhani.txt`. The source font is
`lib/PortableApps/quick_fonts/Rajdhani-600.ttf`. This bitmap is a derivative font;
retain the license and attribution if it is later packaged.

## Native composition contract

Required compile defines:

```
PORTABLE_SERIAL_MONITOR_APP
PORTABLE_SERIAL_STREAMS
PORTABLE_NATIVE_CUSTODY_FENCE
PORTABLE_CHECKED_STORAGE_RUNTIME
PORTABLE_APP_LAUNCH_GUARD
PORTABLE_APP_OWNS_TOUCH_CHROME
```

The app links `Apps/serial_monitor.c`, `PortableSerialClient.c`, the existing
native `adapter.c`, and an explicitly supplied
`portable_serial_monitor_instances`/`portable_serial_monitor_instance_count`
catalog. The fixture's instances 41/42 are fake inputs, not X4 identities or
product grants. Do not ship them. Use the exact canonical Runtime headers at
`6a3b3d46f5b2857eacc6e9863799dec1f466b036`, including `RiscRealtimeV1.h` needed by
the checked adapter. Do not replace the repository's bundled compatibility SDK.

The adapter uses its existing display/touch/battery capabilities. The monitor
uses the Runtime's stream-only client and exact `serial.port` instance grant.
Every call stays on the invocation's owner task. Each UI iteration uses one
bounded client tick, at most `PSC_CHUNK` bytes, and a 20 ms app-allocated stream
control budget. This is a local cooperative budget, **not** a measured physical
callback deadline or a discovery schedule. Stream-driven redraws are paced to
250 ms; navigation redraws immediately when the native frame is ready. RX may
continue while a previous display frame is pending, without writing that frame.

The selected profile deliberately rejects resident, Quick, sleep, Home-route and
return-route defines. No profile or manifest is provided. Full product composition
must establish its own authorized instances, appropriate native launch/return,
safely scheduled discovery, and target callback timing. Do not bypass these
compile checks to claim admission.

## Cleanup and retained custody

`portable_serial_monitor_close()` runs before any native adapter finalization,
including display draining, capability release and allocation cleanup. The
existing launch-guard callback also requires this checked close before a launch
can be queued. Unknown serial closure calls the shared silent invocation fence,
then returns immediately. No diagnostic, render, poll, retry, release or free is
allowed after that boundary. Repeated close/main/fini/launch attempts remain
inert while retained. A special "closing" screen is intentionally absent: close
is synchronous, and displaying anything after an uncertain close would violate
custody. Physical teardown is not inferred from a requested close.

## Reproduce local qualification

```
python scripts/test_serial_monitor.py --runtime /path/to/pinned/RiscRTE --output /tmp/monitor-normal
python scripts/test_serial_monitor.py --runtime /path/to/pinned/RiscRTE --output /tmp/monitor-sanitized --sanitize
```

The runner stages verified canonical headers without mutating the Runtime,
compiles with strict warnings, and runs the actual UI and PortableSerialClient
against deterministic providers. Cases cover unselected/invalid instances,
240-pixel touch pagination through all eight instances/endpoints, exact snapshot
binding, failed refresh, refusal, held/repeated inputs, clean reopen, instance
switch, disconnection, poll interruption, malformed/retained operations and
invocation loss. All 256 byte values, ring bounds, wrapping and printable glyphs
are checked. The unchanged client unit suite is rerun.

A separate fixture links the actual native adapter/controller/client at 240×240
and 480×800 RGB565, with frame bounds guards and captured pixels. It checks
selection, receive, clean refusal, RX during a 600 ms asynchronous immutable
frame, retained open/tick/close, interrupted active-session finalization and
launch cleanup. Every Runtime/provider call after a terminal result aborts the
fixture. Retained frames remain byte-identical. Nine unqualified compile
compositions must fail. The unchanged legacy, checked-native and checked-native
terminal adapter profiles preprocess identically to the parent source when the
monitor is not selected. This is source equivalence, not a release-byte rebuild. Test-only `setjmp` interruption is not production code.

The output contains exact source/header hashes, commands, logs, native frame
PNGs and a contact sheet. ASan/UBSan run with LeakSanitizer disabled because this
execution environment uses ptrace; no leak qualification is claimed.

## Remaining gates

- Full native controller/IDF/Xtensa/ELF target build and callback timing.
- Safe out-of-callback discovery and role integration.
- Product utility placement, instance grants, launch integration and manifests.
- Authoritative X4 VBUS/role-root evidence and physical acceptance.
- Any publication or release authorization already on hold.

No driver/controller source, PortableSerialClient implementation, accepted
builder/profile/manifest, SDK baseline, product pin, firmware byte, remote branch
or release is changed. Drivers remain at
`77980f897e49829a8986df323024132c759671f2`. No hardware, USB, radio, flash,
installation, personal Cloudflare or Mycelium action is part of this work.
