# Selected-volume Open handoff

The optional `--source-handoff` profile retains the same Files row, action,
Open-with, status, Back, and cancellation controls. It adds no transfer panel.
Ordinary primary `storage.volume` SD Open still uses unchanged `file.open@1`.
Other local volumes and a remote mount on a non-SD product use the additive
`file.open.source@1` Runtime capability. File associations still come from
admitted application manifests; unsupported receivers are reported explicitly.
An ELF remains an installation input, not an executable launch path.

Files reads or receives the selected source in at most 4096-byte chunks. One
local chunk is processed per foreground tick, with normal input polling and
cooperative progress. The existing WebDAV transaction controller uses a narrow
in-invocation staging adapter. A sealed source is handed off only after peer
transport and private Wi-Fi cleanup; no app-owned callback, volume pointer or
file handle survives caller unload. Runtime then owns the temporary source and
allows only the selected receiver to read it. Local originals and remote
originals are never overwritten or renamed by Open.

The receiver must explicitly declare and receive `file.open.source@1`, use
`source_info` and bounded seekable `read_at`, and preserve terminal custody.
Existing path-based receivers remain available through ordinary SD Open.
Native source support additionally requires the selected Runtime's trusted
scratch backend; app grants alone cannot invent it. Watch integration selects
one hidden private temporary file in the existing appdata filesystem. Available
free storage limits staging. The current WebDAV protocol has no HTTP Range
implementation: remote files that cannot fit available temporary storage cannot
be opened through this source path. Browsing, Preview and normal Copy remain
available. This is a storage-capacity limitation, not a full-file RAM limit.

Run `bash scripts/test_file_browser_source.sh` and
`ASAN_OPTIONS=detect_leaks=0 SANITIZE=1 bash scripts/test_file_browser_source.sh`.
These execute production Files/controller, the existing authenticated
WebDAV client/server and synthetic capability/network backends in RGB, paper,
and scrolling-paper profiles. Cases cover declared handler choice, local and
BLE-negotiated private-network remote source, unavailable capability, unsupported
receiver, full storage, prolonged BUSY, cancel, and original-file preservation.
The capability stub checks that handoff occurs only after transport quiescence.
This controller fixture is separate from actual Runtime lifecycle and native
scratch-backend qualification. Hardware radios, displays and flash are not
qualified by these host checks.
