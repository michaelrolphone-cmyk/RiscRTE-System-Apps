# v1.6 recovery qualification

Software verification does not qualify physical power-bank behavior.

| Check | Evidence required | Current status |
| --- | --- | --- |
| Boot reset diagnostics | Native compiled API and health@3 fields | Software tested; physical pending |
| Bounded execution hang | Native 90s watchdog and 30s stalled-phase feed cutoff | Compiled/host phase test; physical pending |
| Wi-Fi loss | AP off/on, automatically resumes saved network | Host regression; physical pending |
| Long backlog | 12+ hour outage then ordered catch-up and continuing health | 18,000-event software fixture; physical pending |
| Failed destination | Original identity, backoff, other subscription progress, dead-letter | Existing real relay state-machine/sanitizer tests; physical pending |
| Power removal | Repeated unplug/replug, retained config/keys/subscriptions and original custody | Atomic journal/migration tests; physical pending |

For the physical run, retain `relay.identity` and `relay.health` v3 messages
before and after every scenario. Record boot count/validity, reset reason,
free/minimum heap, largest free block, Wi-Fi RSSI, last acknowledged sequences,
queue depth and per-subscription failures. Do not expose credentials, URLs or
private backup bytes. Heartbeat spacing is three minutes while connected.

Run 10 unplug/replug cycles with the power bank. Confirm automatic boot and
identity publication after each, using the same approved subscription/key.
Switch the access point off for at least a minute and restore it. For the long
outage, leave the relay offline for 12 hours while recording actual elapsed
time, then measure time to current events; do not substitute synthetic event
creation times for an elapsed physical outage. Fail a controlled destination,
verify other subscriptions and health continue, then restore it and verify
at-least-once original-identity delivery and receiver deduplication. Keep the
existing enrollment approval flow and production receiver unchanged.

Use the preserving repair bundle on an enrolled relay. The full initial image
contains an unprovisioned app-data partition and must never be used as an update
for the enrolled device. Preserve the private pre-upgrade snapshot and keep its
raw bytes out of shared evidence.
