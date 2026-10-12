> Historical paused checkpoint. Resumed work is completed and locally qualified at cbf8c5d50b70068db8bd47f331ac7167c79c6ed0; see STATUS.md and the final recovery receipt.

# Confirmed replacement work paused, 2026-10-12

Paused for the user's higher-priority remaining deep-wake timing investigation.

The four-file production diff after b8a3a1fa34c586e4e0e47f78408da6a8840af298 is an untested work in progress. No test or target build has run for this diff. Prior c94bb08/b8a3a1f receipts apply only to that earlier source.

Implemented locally so far:
- Explicit Client::PutMode::ConfirmedReplacement sends If-Match: * only for a PUT without an exact revision.
- Default PUT still sends If-None-Match: *.
- RemoteVolume's explicitly confirmed replacement performs HEAD first, preserving a returned exact ETag. Only absent ETag selects the new mode; invalid/weak returned tags still fail rather than falling back.
- Normal browser confirmation text no longer promises a revision on every backend.

Remaining before qualification:
- Update the existing generic-SD refusal expectation and add actual generic staged replacement success tests.
- Cover confirmation refusal/default exclusivity, cancellation before HEAD, between HEAD and PUT, during upload, missing destination after HEAD, 4xx/5xx/transport failures, no automatic replay, and source-handle cleanup.
- Preserve Watch exact-revision/stale-write behavior and confirm malformed/weak ETags never trigger a wildcard retry.
- Review finding: the generic server's replace_file failure returns HTTP 500 and preserves recovery state; report unknown publication accurately rather than claiming a definite failed write. This follow-up is not implemented yet.
- Run focused normal/UBSan browser and protocol tests and fresh target builds.

Original Reader facts verified at 34d8e694 and 45cf61ac: recursive folder Copy, cross-volume Move, and overwrite were not offered. Generic SD server replacement does not require a strong revision; its existing staged replace_file callback supports If-Match: *. The client limitation is being completed, without recursive-folder feature expansion.

Combined source still contains the separately blocked alarm-lease payload. No combined publication, alternate publication route, firmware assembly or hardware claim. Generic Watch non-SD declared-handler Open remains a separate unfinished Files requirement.

Additional stopped read-only review findings:
- Keep the copied ETag buffer: Client::request resets its internal etag_ before copying match, so passing client.etag() directly would alias cleared storage.
- Test late cancellation in RemoteVolume::Close after successful PUT. Current cancellation can label the already-applied write Failed with an empty error and uncertain=false. Outcome reporting must remain honest and must not retry or undo the write; correction is not implemented.
- Cover owner loss, retained close, unavailable atomic source cleanup and default-create reset after one confirmed replacement.
