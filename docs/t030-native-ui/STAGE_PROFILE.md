# Bounded app pipeline telemetry

Compile the shared adapter with `PORTABLE_STAGE_PROFILE`. The runtime must expose
both its optional `monotonic_ms` and `trace` suffixes. If absent, observation is
inert. A failing observation clock disables profiling without failing the app.
No telemetry callback follows a failed or retained invocation.

First-frame milestones use the existing trace vocabulary with interaction ID 0
(inherit current interaction) and value 0:

- Phase 4: first clear/begin entry
- Phase 5: first accepted async submission with a nonzero token
- Phase 6: first observed async COMPLETE state

Local counters accumulate count, total milliseconds, and maximum milliseconds.
A summary is emitted into Runtime's bounded RAM trace ring at render/poll safe
boundaries at most once per 2 seconds when work exists, even below 1 FPS. One
final summary is emitted after the display drain on ordinary exit. There is no
minimum frame count, new background worker, log formatting, or synchronous I/O.
Runtime's existing bounded `perf` replay controls output transport.

Each phase-8 record packs `(tag << 24) | value`. The low 24 bits saturate at
`0xffffff`; treat that endpoint as at least that large, not an exact value.
For every summary all 15 records appear in this order, including zero counts:

| Stage | Count tag | Total-ms tag | Max-ms tag | Scope |
| --- | --- | --- | --- | --- |
| Render | 0x40 | 0x41 | 0x42 | clear/begin through present entry |
| Poll | 0x43 | 0x44 | 0x45 | full poll_input invocation |
| Catalog | 0x46 | 0x47 | 0x48 | explicit refresh after display drain |
| Display | 0x49 | 0x4a | 0x4b | accepted async submit to observed COMPLETE |
| Input edge age | 0x4c | 0x4d | 0x4e | reduction time minus new ordered event timestamp |

Render includes frame acquisition/clear, rasterization, Contexts capture and
cooperative display pumping during that span. It excludes present-copy and
submission work. Poll includes wait, provider progress, optional services and
input routing, not only touch work. Display duration includes both physical
progress and scheduling until the client observes completion. Input age is
recorded only when a new ordered edge advances the reducer; idle levels are
not counted. It is not a claim of sensor-to-photon latency.

App durations use the runtime's wrapping millisecond clock. Native display
aggregates may separately expose microsecond precision. Do not add overlapping
render/poll/display intervals as if they were disjoint. Summaries are bounded
and may be lost with the existing trace-ring capacity; a decoder must reject
partial snapshots and identify incomplete groups.
