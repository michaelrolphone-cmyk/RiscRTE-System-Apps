# Shared copied-preference pending frame

`PORTABLE_QUICK_PENDING_FRAME` is an explicit foreground-adapter selector. It
requires native capability-safe Quick and the copied-preference controller.
Home has its own qualified controller; this does not change it. Selector-off
historical profiles keep their existing preprocessed implementation.

The shared controller previously submitted a copied write from `quick_apply`
before its next renderer pass. A visible preview therefore could survive an
unknown result without identifying that saving was unconfirmed. The selected
barrier prepares the exact frozen value, marks `save_pending`, completes any
older submission, paints a new unconfirmed image, then waits for that image's
actual frame-idle boundary. An older ready observation cannot authorize this
write. It retains the last displayed time/battery labels without new reads.

While the operation reserves input, dispatch independently records new or broken
contacts as cancellation; it does not rely on the short-circuited `pqa_input`.
Current held contact, malformed/resync authority, Back/Home, handoff and alarm
preemption also cancel before submission. No additional edits are queued or
silently replayed. Cancellation safely returns an unconfirmed failure to the
existing session controller. Provider/frame uncertainty retains the invocation
and leaves the last pending display untouched; no write or speculative repaint
is attempted. The ordinary post-submit controller still owns token collection
and explicit cancellation after native custody has begun.

Owning tests separate the pure completion state machine from actual adapter raw
input/frame composition. Product Runtime tests additionally require a completed
pending frame before the actual storage.preferences submit, then test both a
confirmed shared preference and an unknown successful-collect result. Host
provider media and Runtime instruction mapping are modeled, not physical proof.
