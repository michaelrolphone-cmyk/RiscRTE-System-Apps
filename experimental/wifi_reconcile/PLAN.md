# App-owned Wi-Fi reconciliation prototype

Unconnected model based on published System 83f0b48d. No live ABI, Wi-Fi UI,
Runtime routing, SD writer or .90 product change.

An explicit Save/Forget intent identifies an SSID and optionally the SHA-256 of
the known prior canonical logical record. Local numeric slots never identify a
record across replicas. Forget retains only a fingerprint, not an old password.
The versioned 160-byte envelope validates reserved fields, strings and CRC;
CRC detects corruption, not malicious modification. Credentials remain plaintext
and must never be logged. SHA-256 is supplied by the existing vendored tinyDTLS
implementation; no new crypto implementation or external dependency is added.

The resolver scans an immutable staged namespace6 view, at most one logical
profile per call, and produces a slot plan plus the view revision. It never
writes. Duplicate SSIDs, a changed expected record, or an unknown-base edit that
collides with a different existing record require explicit conflict resolution.
Already-applied identical saves and absent forgets are idempotent. Corrupt or
unreadable profiles stop resolution; they are not silently skipped. Fresh saves
allocate from the restored collection's empty slots/count, never a stale offline
count. Every terminal result and cancellation wipes the copied intent; Scan destruction also wipes abandoned pending state. The original persisted intent remains sensitive and separately owned. Even a Forget intent’s unsalted expected-record digest is sensitive: an observer can test password guesses against a known SSID. Never expose it as public metadata or log it.

Remaining integration: a bounded staging KV adapter must run the existing
credential codec against this view and atomically commit every generated count,
chunk, selector and metadata mutation with the original opaque intent ID. The
native revision/frontier compare must reject concurrent changes. Resolver code
must remain application-owned. Runtime must not interpret SSIDs or profile slots.
No native/SD atomicity, live conflict UI, physical-device behavior or end-to-end
settings persistence is established by this model alone.
