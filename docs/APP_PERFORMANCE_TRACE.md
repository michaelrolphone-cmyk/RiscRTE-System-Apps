# Opt-in app performance traces

The diagnostic builders select Runtime commit
`dfc0af505eb9a63092871f35e60ab2716a91d8f9` with
`--performance-runtime-repo <local Runtime checkout>`. Clock requires its existing
sparse/desk profile; Springboard and Settings require their native time profiles.
Diagnostic versions are Clock 0.3.7, Springboard 1.7.4, Settings 1.3.10. Normal
versions and SDK pins are unchanged. No app is installed or published by these
builders.

The optional `--performance-display-sdk <directory>` stages exactly
`RiscDisplayOutputV1.h`, `RiscDisplayOutputPowerV1.h`, and
`RiscDisplayOutputMetricsV1.h` and enables read-only provider metrics. The selected
X4 SDK preparation supplies this tagged suffix; no chip-specific driver API is
invented by the app. Missing/older suffixes are harmless.

`performance-sdk/sdk` preserves the complete immutable Runtime SDK, and
`performance-sdk/include` is the actual coherent compile tree. The build record's
`performance_trace` identifies the actual Runtime revision, source hashes,
compiled header hashes, builder hash, display SDK hashes and explicit deployment
header overrides. Clock's storage sleep extension is an attributed override of
Runtime's shorter storage header. Normal native Springboard's SDK remains
30dcec5; a diagnostic native receipt reports dfc0af5 and the compiled diagnostic
headers. Settings similarly preserves its ordinary 602ae9b pin. The old SDK
receipts are never renamed as new source.

## Reading a diagnostic snapshot

After one operation, send the exact `perf` command plus newline on the Runtime
diagnostic transport and retain the complete `RTE_PERF begin`/`RTE_PERF end`
block. The optional Runtime recorder owns the bounded ring and does not write
serial data while recording. Its `lost` count indicates overwritten records.
An incomplete interval cannot establish elapsed time. Existing sparse Clock
`SPARSE_CLOCK ... elapsed_ms` lines remain cumulative from sparse startup;
subtracting a native-recovery line from zero does **not** measure RTC recovery.

App trace calls do not allocate, format text, sample another clock, acquire or
release a grant, progress providers, or yield. Normal builds compile the hooks
away. Trace callbacks are version/size/null guarded, including zero-filled bounded
copies of optional Runtime tables in diagnostic builds. A retained/faulted adapter
emits no further events. Physical performance is unqualified by host tests.

## Correlation and milestones

A neutral-gated first accepted software touch sample begins an interaction.
Physical touch time is unknown: the trace begins after the input provider's
poll/event-drain/snapshot has returned, not at an electrical touch edge. Repeated
movement samples only increment a counter. Navigation/Home edges begin an
`other` interaction. Recognition and controller dispatch identify the action;
Settings' actual Next footer, previous footer, and swipe page path are covered.

- Phase 1: software input begin, kind 1 touch or 3 navigation
- Phase 2: adapter-delivered input or controller-recognized action
- Phase 3: controller dispatch
- Phase 4: first raster acquisition of this interaction/invocation
- Phase 5: presentation submission attempt, before the provider submit call
- Phase 6: provider-confirmed completion (value 1), or identical-frame skip (0)
- Phase 7: completed in-app action or incoming app's first completed frame
- Phase 8: schema marker, software sample marker, or encoded counter
- Phases 60/61: inclusive paired stage spans

Launcher feedback completes without ending correlation. The next app inherits
ID zero through Runtime, and its first completed frame ends that same interaction.
Each outstanding frame captures its interaction ID; an older frame finishing
after newer input is never attributed to the new touch. Coalesced or cancelled
interactions may have no completed frame, and beginning fresh input replaces
that correlation. Rejected launches and boundary actions can likewise be followed
by another input without a completion; missing endpoints are not zero latency.
Presentation completion is the provider's status, not a claim about when a user
perceived the pixels.

Stable numeric labels are in `PortablePerformance.h`: 0x100 software sample,
0x101 adapter-delivered input, 0x110 launch, 0x111 failed launch, 0x200 Settings
activation, 0x201 previous timezone page, 0x202 next timezone page, 0x203 timezone
swipe, 0x205 region-to-city, 0x300 launcher page/selection, 0x400 Clock launch.
No coordinates, preference values, strings, file contents or pointers are logged.

Stage tags: 0x1000 surface acquisition; 0x1001 raster; 0x1002 damage calculation;
0x1003 provider submission; 0x1100 promotion plus foreground adapter activation;
0x1101 configuration reads; 0x1102 cold-logo drawing/presentation;
0x1103 native RTC recovery; 0x1104 boot classification; 0x1200 timezone page
state dispatch. Clock's logo span ends before recovery begins. A skipped logo is
a short span. Spans are inclusive and nested: do not add child time to parent
elapsed time. Failed/retained exits deliberately leave incomplete spans instead
of making unsafe calls or inventing successful exits.

## Phase 8 encoding

Value 1 is the app schema marker. Value 0x100 labels the first software sample.
All counted values use `(tag << 24) | min(value, 0xffffff)`.

Tags 1–6 are saturating **cumulative per-invocation** counts: input reads, native
realtime reads, KV reads, raster passes, battery reads, presentation status/wait
calls. Snapshots occur at input boundaries and completed frames. Subtract two
snapshots within the same invocation for interval counts; do not sum cumulative
snapshots or compare across invocations. KV instrumentation covers native
Settings/toolbar wrappers and Clock configuration reads; it is not a claim to
count every storage read performed internally by all services.

Optional display metric tags are **per completed token**, not cumulative:

| Tag | Meaning |
| --- | --- |
| 16 | transferred bytes |
| 17 | scoped GPIO write calls |
| 18 | transfer start to transfer end, ms |
| 19 | queued to BUSY done, ms |
| 20 | effective update rectangle area, pixels |
| 21 | update mode: 0 full, 1 partial |
| 22 | BUSY assertion to BUSY done, ms |
| 23 | refresh command to BUSY assertion, ms |
| 24 | queued to transfer start, ms |
| 25 | provider valid timestamp bits |

Metrics require the exact completed token and current interaction. Durations are
emitted only when both timestamp validity bits are present and ordered. Unknown
intervals are omitted. The getter does no hardware or clock I/O. These durations
are independent of app raster and Runtime loader spans.

## Verification

`test_paper_performance_trace.py` runs the real Springboard controller and adapter
with trace enabled, callback disabled, and an older Runtime table, normally and
under ASan/UBSan. It verifies first-sample/dispatch/draw/submit/complete order,
launch feedback correlation, incoming-frame end, stale completion rejection,
replacement/cancellation and retained faults. It also runs actual native Settings
Next-page, async input and retained-fault flows. With `--display-sdk`, it verifies
exact-token metrics, valid-time filtering, old suffixes and getter purity.

`test_sparse_clock_performance.py --clock-build <diagnostic Clock directory>
--x4 <selected X4 checkout>` runs the production Clock, adapter and selected sleep
hook against strict capability doubles. It checks separate classification,
promotion, config, logo and recovery spans and retained failures. Target builders
check Xtensa ELF layout, imports and exports. These are software qualifications;
a complete real-device trace is still needed to attribute the reported 10–15 s
Settings delay or establish physical timing.
