# Peer Files continuation: TCP connector checkpoint

This is an intermediate source checkpoint, not a completed peer browser.
The delivered X4 0.1.81 / Watch 1.1.2 test packages remain unchanged and server-only.

The ordinary `network.tcp.listener@1` facade 0.1.1 copies the optional native
connector suffix and preserves its listener ABI prefix. It accepts old native
prefix-only tables and advertises no connector for those. It copies endpoint
requests, owns generated handles, checks native result/handle consistency, fences
unknown status or uncertain close, and closes pending connections during quiesce.
Instance-zero service selectors remain zero. No extra provider slot is consumed.

## Validation and composition

The provider's connector/legacy compatibility, stale activation, copied endpoint,
readiness error, unknown status and failed close cases pass alongside all existing
listener scenarios, normally and under UBSan. The Xtensa provider ELF validates;
its build receipt records imports, exports, bounded BSS and stack frames.
Runtime requires focused connector commit `1a1bd64629081a64de7e582adab51292178b1519`
on `81845e8027b94abc5c7a36a45e0004884ada73b6`; old native firmware remains usable
for server operations, but cannot supply outbound transport.

Still required before new product composition:

1. Bounded WebDAV client with Digest session lifecycle and a remote volume adapter.
2. BLE central discovery and secure descriptor retrieval in the existing shared
   broker provider; preserve peripheral setup and checked HCI/Wi-Fi ownership.
3. A Watch user-data volume isolated from firmware, credentials and other apps'
   private namespaces, with real directory/file management rather than Timecard only.
4. Shared Files peer selection, transfers both ways, and local materialization for
   established file-type app launch, retaining current local file operations.
5. Cross-component cancellation/expiry/authentication/storage-state tests, target
   admission and capacity qualification, then newly reserved product versions.

No peer firmware, hardware interoperability or live heap qualification is claimed.
Do not substitute the source-only connector checkpoint for those remaining steps.
