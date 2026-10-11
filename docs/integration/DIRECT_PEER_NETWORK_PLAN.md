# Router-free Watch / X4 Files integration

The same-LAN implementation is incomplete. The required default peer flow is
BLE discovery and explicit numeric confirmation on both devices, followed by a
temporary WPA2 network and automatic WebDAV browsing. No saved router profile is
required. Same-LAN sharing remains an explicit supported alternative.

Isolated branches: `feat/direct-peer-network` in Runtime, System and Drivers.
Exact integration inputs inspected on 2026-10-11:

- Runtime `537af2821b78192e64360f94413113bef9394024` (integrator 0.2.19), including
  hardware-confirmed acquisition/SD correction `ef7d68718d4f054447ecd79cf008549723eb532b`.
- System `ecc84ec23bfff3e27337903833da9d52b83614d4`.
- Drivers `b001f9203c4961ace2a1a51303b952ef0de6dc42`.
- Integrator X4 `36e6a4cf0aca7438133ce972e8348fb12fbd7aa0` (0.1.87).
- Integrator Watch `4bd23e192b9acfa16bc75d79c83d086ceaec76d2`.

Implementation boundaries:

1. Generic native radio/provider support for temporary AP and station sessions,
   bounded asynchronous transitions, owned restoration and explicit refusal of
   incompatible concurrent owners. Credentials stay RAM-only and are erased.
2. Additive tagged BLE network offer, carried only over authenticated encrypted
   Secure Connections after mutual numeric confirmation. Old endpoint-only
   descriptors remain valid for same-LAN operation.
3. Shared Files orchestration: sharing device offers AP credentials and WebDAV;
   browsing device joins automatically and mounts the remote volume. One finite
   session owns setup, address readiness, serving/browsing and ordered teardown.
4. Production state-machine tests cover refusal, setup failure, cancellation,
   timeout, reconnect and restoration. Verify real native target compilation and
   full selected app/provider admission before composing diagnostic increments.

No persistent profile writes, nonzero Runtime software selectors, release/tag/
catalog writes, merges or hardware operations. Preserve all current capacities,
SD-first work, Reader file-type launches and shared Nova/paper controls. The sole
release integrator receives focused source commits and exact build receipts.
Router-free completion requires the full above path; AP scaffolding alone is not
delivery and hardware behavior must be reported separately from software tests.
