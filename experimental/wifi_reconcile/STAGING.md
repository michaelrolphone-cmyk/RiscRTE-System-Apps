# Bounded Wi-Fi reconciliation staging

This second experimental checkpoint builds on the app-owned resolver and existing
PortableWifiProfiles/PortableWifiCredentials codec. It remains disconnected from
live Wi-Fi UI, public ABI, native routing, SD transport and the .90 product.

## Contract

`Stage` accepts an immutable namespace-6 view with a nonzero revision, captured
pending-operation frontier, and an original pending operation ID plus its exact
160-byte intent. The view revision identifies the particular held staged view;
it is not an SD generation. The caller owns the view lifetime and must hold its
snapshot/export lease until staging ends. Calls are serialized on one owner;
source and target callbacks must not reenter the Stage or retain its pointers.

The source must provide side-effect-free reads. Its put callback is discarded and
replaced with a refusal callback. Resolution reads at most one profile per step.
After choosing a logical slot, a separate build step runs the existing codec on a
fixed RAM overlay. That step performs only the codec's fixed number of reads and
writes. A source BUSY clears partial mutations and retries; a source fault,
corrupt profile or capacity failure clears sensitive state and publishes nothing.
No target callback is called until explicit `commit` after `Ready`.

A batch contains up to eight 64-byte values with 15-byte keys, the namespace,
revision/frontier, original operation ID and entire original intent. It is
compile-time limited to 4 KiB and allocates no heap memory. This is an operation
size bound, not a profile or namespace key-count cap. Current operations use:

- New appended Save: profile count, two chunks, selector (four mutations).
- Save into an empty or existing slot: two chunks and selector (three).
- Forget: tombstone selector and zero overwrites of all four chunks (five).
- Already applied Save or absent Forget: zero data mutations, with a resolution
  marker still required for the original pending operation.

The batch preserves the restored collection count and reuses the resolver's slot.
It never uses a stale offline numeric slot/count. Duplicate SSIDs or a conflicting
known record remain conflicts. Runtime sees opaque original intent and generic
key/value mutations; Wi-Fi identity and codec rules remain app-owned.

## Required atomic receiver

The included receiver is a deterministic test model, not a durable integration.
A real receiver must perform all these checks and effects as one transaction:

1. Validate authorized namespace ownership, a currently held immutable view, and
   exact current revision AND pending frontier. A changed revision/frontier means
   `CommitStale`, with no publication, so the app must resolve a fresh view.
2. Verify the original operation ID exists in the captured pending set, its exact
   opaque bytes match, and it has not already been resolved in this candidate.
   An ID being numerically below the frontier alone is not proof of membership.
3. Atomically publish every staged mutation plus a resolution marker bound to the
   original ID and candidate. Never expose intermediate count/chunk/selector data.
   Preserve the original pending group and intent until the resulting SD snapshot
   is durably acknowledged. A resolved marker is not an SD acknowledgement.
4. Return `CommitBusy` only when no transaction or external mutation began. It is
   retryable or cancellable. `CommitIo` means unknown old/new durable outcome; the
   Stage wipes its batch and returns `Uncertain`, requiring reload/inspection.
   It never automatically replays an uncertain commit. A late cancel preserves
   `Committed` and `Uncertain` rather than claiming to have undone publication.

The existing experimental native core does not yet implement this receiver or
resolution-marker storage. A bridge must reconcile all pending groups in order,
retain unresolved/newer groups, validate the complete candidate, and switch the
native root atomically before considering it locally available. It must not drop
pending groups merely because these app-owned staging operations completed.

## Sensitive data and limits

Intent bytes (including prior-record digests) and codec chunks are sensitive.
Terminal results, cancellation and destruction wipe Stage-owned copies. Partial
read/build buffers and codec scratch are also wiped. Source views and original
persisted intents remain separately owned; callers and the eventual receiver
must wipe their copies. These checks do not establish secure flash erasure,
encryption, hardware stack remanence or target task scheduling/latency.

The view may contain more records than a native replica's available byte capacity.
Staging does not claim those records fit the separate experimental 65,512-byte complete-image native
budget, which also includes pending groups. The receiver must admit the entire candidate plus pending data or fail with
`CommitCapacity` while preserving the readable native replica and every intent.

## Verification

Run from the repository root:

```
./test/run_wifi_reconcile_model_test.sh
./test/run_wifi_stage_test.sh
SANITIZE=1 ./test/run_wifi_stage_test.sh
ASAN_OPTIONS=detect_leaks=0 SANITIZE=1 SANITIZERS=address,undefined ./test/run_wifi_stage_test.sh
```

The staging suite covers 1,025 existing profiles, offline slot remapping, reusable
holes, count preservation, update/forget mutations, duplicate/conflicting logical
identities and operation IDs, exact opaque intent retention, revision/frontier
CAS, zero-mutation resolution, cancellation, late cancellation, and buffer wiping.
Its 1,006 injected capacity, mutation, ambiguous-commit, read-failure and BUSY
cases exercise all source reads for new Save, update and Forget, including BUSY
after a count was partially staged. Every case asserts no source writes before
commit. Failed atomic receiver cases leave either the complete old or complete
new map, never a partial map. Only synthetic credentials are used. A strict Xtensa GCC 8.4 C++17
compile of the staging entry points also passes with exceptions and RTTI
disabled; this is an object/header check, not a linked device build.

Native receiver integration, durable resolution markers, SD reconciliation,
public intent/group ABI, live conflict UI, connection and device power-cut
qualification are still outstanding.
