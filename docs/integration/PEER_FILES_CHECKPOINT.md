# Peer Files development checkpoint

Current isolated System lineage includes `c2bed31` (installed-package lifecycle
and Home hydration); Runtime includes `6c65814`; X4 includes `80b9008`.
No installer admission rule or legacy setting was relaxed. Software selectors
for export, TCP, entropy and BLE setup remain zero.

The optional `--peer-files` profile adds shared Nova/paper nearby-device selection,
numeric comparison, remote folder listing, upload/download, explicit replacement,
rename/move/copy/delete requests, and download followed by the existing declared
file-type broker. Work is tick-driven with bounded transport buffers. This is
an in-progress integration, not a firmware release or hardware qualification.

Tests run in this workspace: two production NimBLE brokers complete SC numeric
comparison and GATT descriptor exchange; reject and expiry fail closed. The
copied descriptor feeds the actual WebDAV client against the actual Digest
server and production Runtime AppData export mapper. All eight existing Watch
file mappings are read/replaced; stale revision writes are rejected and private
paths do not reach the backend. Two generic storage volumes also exercise
bidirectional transfer, persistence, PROPFIND and mutations under UBSan.

Watch and X4 peer-enabled Files target ELFs have compiled and passed structural
validation, preserving their existing feature selections. Pending qualification:
frontend lifecycle/launch tests, broader failure cases, final source-hashed
component rebuilds, graph admission and resource-budget reconciliation. No new
full firmware is produced at this checkpoint. No device or network is accessed.

See `WATCH_USER_FILES_SCOPE.md` and `watch-user-files-export.json` for the exact
existing Watch files. The optional experimental documents provider is not used
as a substitute for those files and is not selected in product composition.
