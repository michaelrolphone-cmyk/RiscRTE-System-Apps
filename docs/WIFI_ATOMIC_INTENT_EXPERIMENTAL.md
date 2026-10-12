# Experimental local atomic Wi-Fi intent

This source is based on complete System product `.99` baseline
`49752a02746b38afbcc403bb92189be5a8356c9d`. Files, mounted peer browsing, OBD,
Home and existing product builds are preserved. No production `shared_settings`
selection or deployment recipe is changed. Watch has no SD and retains its
existing KV1 behavior.

`--experimental-atomic-wifi` is an explicit X4-native-time resident build flag.
It acquires the separate namespace-6 `storage.key-value.atomic@1` contract.
Missing/invalid atomic capability never falls back to the old KV1 writer. The
normal flag-off build does not acquire or import the new interface.

The app scans one profile per step under a fixed native revision, rejects
unreadable/duplicate SSID datasets, captures an expected logical record, and
stages the existing profile codec's count, chunks, selector and cleanup writes
in RAM. One asynchronous submission copies the complete map mutation batch and
160-byte WFI0001 semantic intent. Existing intent bytes and SHA-256 fingerprint
encoding match the prior app-owned reconciliation model; numeric slots are never
persisted as semantic identity. Existing `Stage` remains a rebase resolver and
is not used to publish new local edits.

Save always records an explicit operation, including same-value saves. Forget
captures logical identity before confirmation; a changed local revision aborts
without replaying its old numeric slot. Unreadable slot deletion is deliberately
unavailable in this opt-in path because there is no trustworthy logical identity.

Remember prepares a local operation before Connect, does not submit any write
before connection success, and commits the same captured operation afterward.
An intervening edit causes explicit stale failure rather than replacing newer
credentials. Cancelling before submit discards only RAM. After submit, Back or
close must collect the terminal receipt; cancellation can lose to publication,
in which case the app reports the committed save. No uncertain result is
replayed automatically. Native success does not claim SD backup confirmation.

## Verification

- `python scripts/test_wifi_atomic.py` (19 controller cases, fake scheduled native endpoint)
- `python scripts/test_wifi_atomic.py --sanitize`
- `python scripts/test_wifi_workflow.py --output /tmp/wifi-legacy-workflow`
  (56 unchanged workflow cases and 50-profile legacy collection coverage)

This System checkpoint contains the C contract, bounded app staging, live
controller binding, explicit build path and tests. It does not yet qualify an
actual registered native broker or physical X4 storage. The disconnected native
Runtime adapter and durability tests are a separate checkpoint. No hardware
access, flash, release, SD data mutation or publication is part of this work.
