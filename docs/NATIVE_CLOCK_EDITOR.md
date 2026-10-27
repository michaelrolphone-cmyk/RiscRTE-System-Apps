# Explicit native clock editor integration

`--clock-edit-authority` selects the tagged API2 editor extension. Native-only
Settings alone dispatches editor operations; every selected API2 consumer must
accept the extension feature. The historical SDK anchor, descriptors, builders
and target code remain selected when this option is absent. No legacy suffix
is cast as the tagged editor interface.

An explicit user Save prepares the native UTC request, calls the service's
checked durable begin, then performs exactly one native seed/readback and closes
the native grant. Only verified success dispatches one finish. The service
verifies native time independently, durably replaces authorization and reconciles
unchanged UTC schedules/occurrence history. An active/dismissing/output-uncertain
alert refuses before clock mutation. The user can dismiss the real alert and
then retry Save. Ordinary forward and backward edits are not prohibited merely
because schedules exist.

A clean inability to begin the native write may invoke checked cancel before
any seed attempt. An attempted but unverified seed is different: preserve durable
INTENT and retain the invocation, never blindly restore old authorization or
retry the clock write. Restart is required to enter explicit recovery. Unknown
provider results, failed release and retained custody stop further callbacks.

Clock and NVS writes are not atomic. An interrupted durable INTENT is an explicit
unverified recovery state after a fresh service/Deep process, not a committed
edit. Its no-occurrence RTC error can be dismissed with Back; a deliberate fresh
Settings Save resolves it. Product tests must cover actual Home→Settings routing,
not merely direct controller calls. The service-only seventh key and existing
boundKV API2 are required for the316-byte ledger; app Settings gains no direct
ledger or occurrence write grant.

A VERIFIED ledger authorizes only its exact schedule identities and UTC lower
bounds. New schedules do not inherit an old authorization; occurrence identity
and generation rollback checks remain. Awake unannounced wall-clock jumps still
fail the monotonic comparison. Cold/default-time ambiguity remains inherent:
if the default placeholder coincides with a previously authorized UTC domain,
this software cannot prove that time is physically correct. It is not clock
synchronization or hardware RTC retention evidence. Accurate manual time and
physical qualification remain necessary.
