# Shared Wi-Fi time-server synchronization

Opt in with `build_portable_wifi.py --time-server-sync`, the existing foreground
alarm/clock-edit profile, and a Runtime checkout containing the canonical
`RiscTimeSampleV1.h`. `--time-server` selects the request server (default
`pool.ntp.org`). The product supplies `net.time-sample@1` and
`runtime.realtime-control@1`; this builder does not apply grants or connect a
network. Existing saved credentials, manual connection and scan workflows stay
owned by the Wi-Fi controller.

The controller starts one attempt after its own successful connection has a
nonzero station IP. An appended **Sync time / retry** row reports pending,
verified or error state and supports explicit retry/cancellation. No background
reconnection, automatic credential writes, timezone changes or periodic sync
are introduced. Existing root row identities are unchanged.

`PortableTimeSync` is invocation-local and read-only. It acquires the sampler,
validates its typed table, copies the result, and closes/releases before a civil
clock commit. `close(PENDING)` retains the provider and network service lease
while native cleanup progresses; Back, sleep, disconnect, new scans and joins
cannot abandon it. RETAINED, CONTEXT, malformed ownership and unavailable
monotonic authority fence all later callbacks. Ordinary network/protocol errors
leave manual clock state unchanged. A 5-second request timeout is enforced by
the provider and client; asynchronous resolver cleanup may take longer, so
cleanup remains visible and cooperative rather than falsely claiming closed.

Only a sample in the supported native UTC range (2024 through INT32_MAX) with
exact precision/flags/structure and at most 1 second of boot-local age can seed.
The public wrapping 32-bit monotonic clock is compared with the low 32 bits of
the provider completion timestamp over this bounded interval. This is not
proof of server accuracy: SNTP is unauthenticated and its 1-second precision is
not a network-delay/accuracy claim. The UI says so even on verified readback.

`PortableClockCommit` is the shared extraction of the pre-existing manual
Settings transaction: alarm editor begin → native control acquire → checked
seed/readback → close → alarm finish and verified edit status. Network samples
are rechecked for freshness after alarm intent and after control acquire.
Expiration cancels the intent before seeding. An attempted but unverified seed
retains custody and never rolls back or automatically retries. Manual Settings
keeps local-calendar/timezone interpretation and its existing save behavior.
The Wi-Fi bridge defers while a frame or alarm modal owns presentation.

## Qualification boundaries

Owning tests exercise the actual shared sampler, adapter bridge, realtime
client, manual Settings path, and real Wi-Fi controller cleanup actions.
Sampler/clock/network callbacks are modeled; the separate Settings service lane
uses the actual durable alarm service with modeled clock/KV. These are software
proofs, not a physical SNTP association, device clock accuracy or whole-product
qualification. Target admission, installed Runtime successful synchronization,
UI review and final firmware assembly belong to product integration.

The user's requested move from operationalization to incubation is conditional
on their confirmation after testing; it has not occurred. Migration of all
remaining apps is the subsequent incubation work, not a claimed result here.
