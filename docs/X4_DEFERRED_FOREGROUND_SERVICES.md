# Opt-in X4 Home foreground radio deferral

Build the delivered full sparse resident Home profile with
`--deferred-foreground-services`. The selected package is Home 0.4.13;
without that flag the existing 0.4.11 profile and generated ELF remain unchanged.
The selected manifest capability requirements are unchanged.

Only the initial Quick radio policy load moves. Clock configuration, timezone,
RTC recovery, touch/navigation, brightness/tone, early alarm service and the
copied Points projection remain in their existing paths. Broadcast stop/pause
is unchanged: an app-local empty client is not proof that a boot-session service
is inactive. This does not claim the whole BLE dependency graph is deferred.

The actual foreground Home renderer sets a memory-only marker after submitting
its scene. Boot animation, loading screens and timer desk frames do not set it.
Normal polling applies Quick radio policy only after this Home frame is idle and
settled. Physical input, sleep, direct launch and the admitted resident loading
callback force checked initialization before the transition. A pending immutable
frame drains first; mutable leases are never used for policy I/O. Quick's existing
settled policy reload also completes pending initialization.

Alarm handling has priority at both pre-wait and post-wait poll boundaries.
Due cues/modals do not trigger optional radio initialization. Existing immediate
low-battery safety policy remains an explicit exception to first-frame deferral,
including its checked radio-off reload while a frame is pending. Warm resident
handoffs retain their original cleanup. Empty saved-continuation probes do not
force radio work; the Runtime loading callback handles actual continuations.

Any ambiguous acquisition, release or control result retains the existing
invocation and permits no subsequent provider work. TIMER returns before
foreground readiness/initialization; its .93 storage-close transaction remains
product-owned and unchanged. There is no new Runtime SDK ABI, provider authority,
GUI, SD layout or alarm/Points format.

Host tests use actual Clock/adapter/Quick-policy source with deterministic native
providers; they are not physical timing or power qualification. The separate
full-profile TIMER suite covers .93 SD closure, retained 408-byte records,
orientations and terminal fences. Physical boot/wake latency remains unmeasured.
