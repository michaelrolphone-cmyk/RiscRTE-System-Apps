# Shared Files: selected removable volume and mapped AppData

The Files sharing screen explicitly selects the current `storage.volume@1`
when that capability is exposed. Otherwise it names and serves the configured
saved-files export. The preserved Watch canvas baseline has installed-file
browsing but no SD provider; its sharing scope remains mapped AppData. X4's
selected SD instance remains 9. TCP, entropy, export and BLE software selectors
remain zero. Wi-Fi/BLE setup, finite sessions, stop and expiry remain shared UI.

`WebDavVolume` consumes only existing volume, checked-state and filesystem
suffixes. No Runtime storage ABI, async request layout or operation number is
added. The install task's separately tagged package suffix (operations 100–103)
and existing async v2/v3 operations 1–7 are untouched. An active installation
worker session reports volume unavailable; WebDAV does not bypass its ownership.

Supported SD methods: OPTIONS, GET, HEAD, PROPFIND, PUT, MKCOL, DELETE, MOVE,
and COPY. Downloads and copies stream 4096-byte chunks. PUT stages up to 64 MiB
without holding the body in RAM, computes its SHA-256 incrementally, and only
publishes after final Digest auth/auth-int validation. Cancellation aborts the
exclusive staging write or removes its closed staging file. Mapped AppData
retains its existing 64 KiB limit and strong revision/CAS semantics.

SD replacement uses the existing filesystem recovery/replacement primitive,
never delete-then-write. Failed replacement does not authorize deleting a
recovery file or either destination. Cross-directory file overwrite MOVE uses
a staged copy followed by checked source removal; failure may leave both full
copies and reports failure. Directory moves use provider rename. Collection
COPY/DELETE traverse with closed parent handles and a 32-level bound; COPY needs
the existing directory seek/tell suffix. Existing destination collections are
refused (409), not recursively overwritten. A cancelled/failed recursive
operation may leave completed children; it is not a whole-tree transaction.

No strong SD ETag is invented from a timestamp. Existence preconditions are
supported; specific If-Match/If-None-Match entity validators are rejected (412).
The AppData backend continues to support its exact strong revision validators.
PROPFIND supports depth 0/1, named/all/name-only properties, escaping and 404
propstats for unknown properties. Its response allocation is bounded to 256 KiB;
excess returns an explicit error. Infinite-depth collection enumeration is
refused. LOCK and PROPPATCH are not implemented or advertised.

Every storage operation is bracketed by pure owner/custody observation. Absent,
exported or unavailable media produce 503; uncertain custody is terminal. Read
handles on unavailable media still receive checked close. A failed close retains
all remaining grants and stops transport/cleanup. No remount, destructive format
or physical device access is inferred. Root deletion and provider recovery/staging
paths are denied. Cross-host Destination and traversal paths are rejected.

Evidence: test_webdav_volume.sh (normal/UBSan), production Session/core/Digest
regressions, actual Files RGB/paper BLE controller/adapter tests, and X4's
`test_shared_webdav_sd.py` against the production async-v3 SD driver and real
shared FatFs with modeled native worker/sector transport. Target Files ELFs are
built independently. None of these is a hardware interoperability claim.
