# Router-free Watch / X4 Files integration

The same-LAN implementation is incomplete. The required default peer flow is
BLE discovery and explicit numeric confirmation on both devices, followed by a
temporary WPA2 network and automatic WebDAV browsing. No saved router profile is
required. Same-LAN sharing remains an explicit supported alternative.

Isolated branches: `feat/direct-peer-network` in Runtime, System and Drivers.
Exact integration inputs inspected on 2026-10-11:

- Runtime `c3d750d113dbc79af87daaad316ab1d63d57dd60` (integrator 0.2.19), including
  hardware-confirmed acquisition/SD correction `ef7d68718d4f054447ecd79cf008549723eb532b`.
- System `ecc84ec23bfff3e27337903833da9d52b83614d4`.
- Drivers `b001f9203c4961ace2a1a51303b952ef0de6dc42`.
- Integrator X4 `24fc5b7e8e7cee9d0150dbca2a4036fc21e2cbc4` (0.1.87).
- Integrator Watch `f5ba7734d4fc36b770bc2edf9fbb9fafdf3a2732` (merged source; unchanged tree from 4bd23e1).

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

Wi-Fi provider input: Watch `a304597bf26491483bb0a567569f0ca0267cdf9c`,
provider 0.2.1, isolated `feat/direct-peer-wifi-provider` branch. Runtime c3d750d
includes the integrator retained-yield correction; the .87 hardware test is pending.

Qualified implementation checkpoints and host/target evidence are recorded in
`DIRECT_PEER_NETWORK_QUALIFICATION.json`. Target Files is 1.5.26; BLE is 0.1.3;
Wi-Fi is 0.2.2; Runtime is 0.2.20. X4 preserves the current 40-provider selection;
Watch retains its 32-provider profile. Full cohort admission precedes image assembly.
