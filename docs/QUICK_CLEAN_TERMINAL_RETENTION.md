# Quick Controls clean-refresh terminal retention

The selected X4 Home package is 0.3.25, following the separate optional-tone
source unit. This does not reserve a product version or publish an image.

A false display submit/status/wait callback during explicit Clean Refresh may
already have retained provider custody. The adapter now installs its silent
fence before any log timestamp, diagnostic, cleanup or subsequent provider call.
The shared display-failure handler is idempotent after the native fence, so a
synchronous loop's trailing timeout cannot log again after a status failure.

A successfully returned FAILED/SUPERSEDED status and local timeout still use
ordinary diagnostics while Runtime is live. Runtime's own terminal notice is
unaffected. Ordinary rendering, dirty geometry, display intent, and the fast
provider are unchanged.

The production Runtime/Graph matrix enables stage logging for all 32 cases in
both normal and ASan/UBSan builds (64 total). New cases exercise wait refusal and
live FAILED/SUPERSEDED statuses; existing submit/status refusal cases forbid later
I/O, focus changes, unmaps and frees. The two live failures each log display
failure and retention exactly once, before the Runtime fence. The same matrix
also runs without stage logging and expects no stage diagnostics.
