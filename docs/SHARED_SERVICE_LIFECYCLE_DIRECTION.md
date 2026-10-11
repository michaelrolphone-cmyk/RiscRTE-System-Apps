# Smallest generic abandoned-session repair direction

This is a design direction, not part of the implemented/qualified checkpoint.

The current Runtime grant is an opaque capability pointer. A boot-pinned provider
can outlive a foreground application. Its global quiesce callback runs on
provider shutdown, not on every application grant release. Consequently, Runtime
cannot automatically cancel one abandoned text session without either knowing
keyboard semantics or gaining a generic per-consumer lifecycle contract.

A small generic repair would add an optional, append-only provider descriptor
extension for grant-owned client leases:

1. On app capability acquisition, Runtime supplies an invocation-generation
   identity and obtains a per-client table/context from a provider-owned factory.
2. Before explicit grant release or app teardown, Runtime invokes the lease's
   bounded cancellation/quiescence operation, even when a boot pin keeps the
   provider loaded. The provider closes its own sessions and drains pending work.
3. Clean completion allows grant revocation and app teardown. Retryable pending
   work preserves the same lease and app invocation until a later safe retry.
   Uncertain custody permanently retains the affected invocation and graph.
4. Revoked client contexts reject later calls. No app callbacks or app buffers
   are retained. Global provider quiescence remains a separate last-owner step.

For text input, the provider-side lease would own the current text/scene/HID
session and cancel it on lease close. Runtime would continue to know nothing
about text, keyboards, layouts, alarms or display rendering. Existing providers
without the extension keep their existing lifecycle semantics.

Before implementation, define identity exhaustion, failed acquisition rollback,
multiple grants by one app, active child/host invocations, retention propagation,
and retry behavior at every application-exit phase. Verify both eager boot pins
and demand loading with real provider/app unload tests. This checkpoint does not
silently introduce that wider Runtime change.
