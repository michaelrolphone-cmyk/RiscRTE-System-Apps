# Optional foreground alarm consumer

`PORTABLE_ALARM_CLIENT` is an explicit deployment-only opt-in. Existing builds
neither request nor acquire `alarm.service`. The canonical consumer table is
copied byte-for-byte from Utilities PR11; `ALARM_PROVENANCE.json` pins it.
The service alone owns schedules, RTC reconciliation, occurrence persistence,
output lifecycle and retry policy. The adapter does not create another scheduler.

Normal `step` calls happen only at the serialized foreground boundary with no
held lease and an explicit completed presentation. The synchronous presentation
loop only polls display/input and yields. Submission marks the boundary unsafe;
only COMPLETE makes it safe. Timeout/FAILED/SUPERSEDED/health failure cannot
be treated as a completed frame. Failure calls `stop_only` at most three times;
unsafe cleanup retains the invocation and grants, emits a one-way diagnostic,
and yields forever without further I/O. Safe cleanup never implies durable ACK.

Reconciliation completes before input dispatch and Back/request_launch. An
active occurrence or new service error takes over an in-place modal. The
original app stack/model/grants remain live. One app-owned full-frame RGB565
copy preserves completed pixels: 115,200 bytes at240x240. The modal reuses that
buffer and does not borrow a provider lease. No outgoing app callback or pointer
is retained. Input is reset on entry, occurrence replacement, action, and after
restoring the image, suppressing initiating and dismissal contacts. Each dismiss
uses the copied current identity; a stale response cannot close the modal. Only
service READY with no active/uncertain output permits normal restoration. A
second simultaneous occurrence remains inside the modal. A blocked error with
no occurrence/uncertain output may be acknowledged visually with Back; its
service remains blocked until explicit Retry and sleep remains unavailable.

`PORTABLE_APP_SLEEP_LOCAL` plus the alarm flag selects a distinct local
`portable_app_alarm_sleep` hook. Deployment must reconcile and consume
`prepare_sleep` immediately before owned timer entry and refresh after returning
wake/refusal. No default generic sleep behavior is silently changed.

`PORTABLE_ALARM_SETTINGS` independently enables Settings' namespace1
`alert_mode` UI. See PORTABLE_SETTINGS.md for persistence details. Neither
option grants namespace4, output APIs or provider storage to an application.

## Checks and limits

`test_portable_alarm.py` executes the actual adapter in ordinary and sanitizer
builds with pending/failed displays, retained cleanup, initiating Back/touch,
exact frame restoration, loading, RTC-error retry, second occurrences and stale
acknowledgments. The broader unchanged portable and application suites remain
required. These host boundaries do not establish physical output quiescence.
Pinned target builds validate the optional Settings and Springboard ELF paths.
The complete Watch integration additionally requires the selected service,
Runtime capacity/storage/output support, owned Light/Deep calls and hardware
qualification. No stable release is repinned by this shared client change.
