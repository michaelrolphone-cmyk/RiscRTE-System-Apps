# Headless agent modes

One firmware supports four persistent roles. `configuration.mode` is optional; absent means `explorer` and retains existing behavior. Select a role in the setup portal with the device's private pairing file, preconfigure it in the pairing policy, or send a higher-revision `infra.configuration` desired document through the existing participant configuration offer. No display, buttons or host computer are needed after provisioning.

| Mode | Responsibility | Work admitted |
|---|---|---|
| Explorer | Existing general infrastructure node | All configured bindings and relay |
| Sentinel | Independent service observations and relay round-trip evidence | Health/source GET bindings; one synthetic challenge every five minutes |
| Reconciler | Recover missed notifications and advance retained work | Existing Scheduler and Capabilities bounded maintenance calls |
| Continuity | Backup clock and shared relay executor | Maintenance bindings plus explicitly configured shared relay |

Bindings keep their existing scoped endpoints, credentials, expiration, cadence and processing budgets. Mode selection never grants new authority. Offers and local dispatch both enforce the role. Unknown modes are rejected. A role change with unresolved participant work or local relay custody is reported as unapplied; drain that work and submit a new desired revision. Applied observations are saved with the configuration before being sent. Wi-Fi-only setup leaves the role untouched.

## Sentinel provisioning

Give Sentinel its own participant identity and a separate Bus bearer binding with `monitor_only:true`, `monitor_id`, no attachments, and the normal enabled/expiration/hash fields. Configure the firmware's `relay` URL and credential reference with that monitor token. The token can only start and inspect its own diagnostic challenges.

Provision a diagnostic subscription on the actual delivery relay watching channel `relay.diagnostics`, type `relay.challenge`, filtering `monitor_id` to this Sentinel. Its fixed webhook destination is the Bus `/v1/diagnostics/receipt` endpoint. Provision an independent bearer credential in the relay and a matching `DIAGNOSTIC_RECEIVER_BINDINGS` entry containing `monitor_id`, `relay_id`, `subscription_id`, `credential_sha256`, `enabled` and `expires_at`. The Sentinel does not receive that credential.

Challenges have a 90-second deadline and 24-hour retention; creation is limited to one per minute per monitor. Sentinel polls every 15 seconds and publishes a durable `sentinel.roundtrip` observation. Bus acceptance remains pending until an authenticated callback for the exact message is recorded. Results distinguish verified, late and missing callbacks. This proves the diagnostic relay path, not completion of a business assignment. Deploy the Bus schema/route before enabling these bindings. Ordinary health bindings continue to use existing endpoints and semantic checks.

## Shared relay and Continuity provisioning

Configure primary and standby with separate participant/installation identities, separate Bus bearer credentials, the same logical `relay_id`, and identical subscriptions and origin allowlists. Each policy adds `relay.shared:{"executor_id":"<unique device id>"}`. Each corresponding `RELAY_BINDINGS` entry adds `shared:true` and that authenticated `executor_id`. Shared bindings must contain only event attachments; all active credentials for that logical group must be shared. Every subscription must declare `deduplicates:true` after verifying the destination honors the stable message idempotency key.

The standby delays its first lease request by 30 seconds. The group lease lasts 120 seconds and is renewed with at least 60 seconds left. Fences increase on takeover, including a new boot. HTTP dispatch is refused unless the monotonic lease has enough time for the complete 30-second request plus margin. ACK and shared checkpoint writes enforce the fence inside their database mutations. A device unable to contact the lease service stops dispatching.

A bounded shared snapshot (12 KB) retains attachment identities, selected targets, attempts and retry deadlines. Snapshot commit precedes dispatch. An interrupted dispatch retains its attempt and idempotency key; a successor treats its outcome as unknown and respects its retry time. Authoritative server ACKs remove already-consumed work during recovery. Delivery remains at least once; a lost HTTP response can require replay at the receiver.

For this version, shared callback credentials are individually provisioned in each device's private pairing file. Shared groups refuse the existing single-executor sealed enrollment flow, which cannot reseal an old secret to another device. Never clone the primary's private enrollment key or participant identity. Subscription changes require a coordinated group configuration update; mismatched configuration digests fail closed. A Continuity device without shared relay configuration still supplies authorized maintenance and reports that relay standby is unconfigured.

The existing serial-owner update and data-preserving repair procedures remain applicable. Software tests and target compilation are separate from physical power-cut, Wi-Fi and long-running hardware qualification.
