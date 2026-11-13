# Failed-service touch resynchronization

The shared reducer previously drained a failed poll, observed GAP and immediately
requested a snapshot. CST9217 invalidates its snapshot when a report is malformed
or cannot be read; the immediate snapshot returns false. The reducer correctly
regards an actually called snapshot(false) as terminal, so this ordering turned a
recoverable malformed report into retained invocation. Existing GT911 may retain
an older snapshot instead; a general consumer cannot assume either representation.

`portable_touch_sync` now defers the snapshot during the known failed-poll interval.
It cancels derived gesture state and waits for a successful service before asking
for authoritative state. Locating the guard in sync covers GAP, empty-queue resync,
and modal/reset boundaries. Queued events still drain one at a time as required by
the public poll contract; they cannot activate controls during uncertainty. A held
recovery is ineligible for tapping until neutral and a fresh gesture. No provider
callback result is altered. Called snapshot(false), next(-2), and other terminal
failures retain their existing invocation fence.

This requires correct provider classification: actual broken provider/sync state
must be reported as next(-2), not conflated with stale subscription/queue GAP(-1).
The paired CST provider correction and its fault tests are an integration dependency.
The reducer must not be deployed as a substitute for that provider fence.

Software reproduction uses actual CST source and shared reducer, with successful
modeled transfers and malformed marker/count/coordinates. Original composition
retains unless a valid service happens before draining. The repaired composition
survives repeated invalid reports and recovers after successful service. This is a
source/composition defect, not proof that Michael's physical ~554ms Springboard
failure was caused by the same report. Actual installed Runtime and target cohort
qualification remain separate gates; no physical result is claimed.
