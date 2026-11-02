# Headless agent modes

One firmware supports five persistent roles. `configuration.mode` is optional; absent means `explorer` and retains existing behavior. Select a role in the setup portal with the device's private pairing file, preconfigure it in the pairing policy, or send a higher-revision `infra.configuration` desired document through the existing participant configuration offer. No display, buttons or host computer are needed after provisioning.

| Mode | Responsibility | Work admitted |
|---|---|---|
| Relay | Existing event-delivery and enrollment role | Relay delivery, retries, receipts and enrollment; no observation/maintenance bindings |
| Explorer | General infrastructure exploration and configured work | All configured bindings and relay |
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

## Automatic role allocation (v1.8.0)

New initial images select Automatic. Existing provisioned images preserve manual
behavior until Automatic is selected in authenticated setup or a provisioning
policy includes `"allocation":{"automatic":true}`. Wi-Fi corrections keep the
existing choice. Each device needs its own participant identity and credentials.

At attachment and on every participant sync, the device supplies eligible local
profiles and its previous assignment's application state. Capabilities fills
priority-ordered role demands using service and feature compatibility. Flexible
devices can move so limited devices cover the roles they can perform. Assignments
are reserved atomically; they count as active only after a matching session and
generation reports a durably applied role. Unfilled demands remain visible.

The lease is at most 120 seconds, conservatively reduced by network time. Losing
contact or rebooting stops new automatically allocated work until renewed. A
changed assignment is journaled as draining, stops new intake, reconciles existing
jobs and finishes relay custody, then atomically saves the selected role and
binding set. Pending human enrollment or an unavailable destination can delay a
switch; this remains draining rather than falsely reporting coverage. Shared
relay effects additionally require the existing Bus fencing lease and receiver
idempotency. Allocation is not proof of completed business work.

`catalog.infrastructure_demand_put` accepts an operation ID, expected revision
(0 for creation), and `demand` with `demand_id`, `mode`, `services`, `features`,
`count`, `priority`, and `expires_at` (Unix milliseconds). A service's authenticated
operator/bridge can publish or renew its demand. Count 0 withdraws it. Different
publishers should use distinct stable demand IDs: counts are additive. Higher
priority wins; equal priorities use stable demand identity. Use
`catalog.infrastructure_status` to inspect reserved, active, gap, unallocated,
and per-device transition reports. Current bounds: 32 active demands, 64 slots,
128 live participants, 8 profiles per device. Status declares truncated coverage.

Default profiles derive services from provisioned `endpoint_policy_id` values,
features from binding capability IDs, and omit unresolved or expired bindings.
Sentinel can include `relay.roundtrip` only with a separate
`relay.monitor_credential_ref`; it uses that credential solely for diagnostic
challenge calls. Continuity requires shared relay configuration, valid static
callback secrets and idempotent subscriptions; its feature is `relay.shared` and
service is `relay:<relay_id>`. An unconfigured board never advertises these roles
as available. Existing manual Sentinel credentials remain compatible.

For finer service selection, provision `allocation.profiles` as an array of
`{profile_id,mode,services,features,binding_ids}`. Each profile names existing local
binding IDs, and its features must be implemented and provisioned. The coordinator
can choose these profiles but cannot inject URLs or secrets. While Automatic is
selected, `infra.configuration` desired updates are declined with `applied:false`;
use authenticated setup to change local scope or return to a fixed role.

Example service demand (one Reconciler for the default Scheduler binding):

```json
{"protocol_version":"1.0","family":"catalog","operation":"infrastructure_demand_put","operation_id":"scheduler-role-demand-1","expected_revision":0,"arguments":{"demand":{"demand_id":"scheduler.reconciler","mode":"reconciler","services":["scheduler-main-endpoint"],"features":["scheduler.drive"],"count":1,"priority":100,"expires_at":1893456000000}}}
```

Host regression and target compilation are software evidence. Physical attach,
power-loss and prolonged network-partition qualification remain separate.

## Five-mode inventory (v1.8.1)

Relay is independently selectable and automatically assignable through the generic capability controller. Its potential feature is `relay.deliver`, scoped to `relay:<relay_id>`. Shared configurations also advertise `relay.shared`; single-executor configurations retain their existing enrollment path. A missing relay policy or credential does not advertise Relay availability. All five modes remain selectable even when some profiles are unavailable on a particular board. Existing configurations without a mode still use Explorer to preserve behavior.

Baseline demand counts are staffing policy, not the inventory of modes. The three initial capability demands do not remove Explorer or Relay. Services can request Relay capacity using `relay.deliver`; the allocator sends a leased Relay assignment and drains existing custody before switching away.

## Mixed roles and failure recovery — v1.9.0

Automatic devices now request a bounded set of assignments using
`infra-allocation/2`. The default maximum is five; `allocation.max_assignments`
may reduce this to 2–4, or 1 for the original single-role protocol. The five
profiles remain Relay, Explorer, Sentinel, Reconciler and Continuity. `mixed`
is a configuration containing selected `active_modes`, not an additional role.

With one demand for each role, fully provisioned devices distribute five roles
as 2/2/1 across three devices, 3/2 across two, and all five on one survivor.
Returning devices trigger load redistribution. Each demand replica uses a
different physical participant. Coverage is limited by locally advertised
credentials, service scopes and capacity. Relay/Continuity failover requires
shared relay custody and receiver deduplication; dynamic enrollment retains
its existing single-executor restriction.

The service produces and persists assignments on attach/check-in. Old-owner
leases expire before a replacement begins. Firmware separately drains durable
pending work, applies the complete role set, and acknowledges its generation
and exact profile IDs. Reboot restores no lease authority. All roles share one
TLS exchange and one job slot, deduplicated binding schedules, existing memory
low-water checks and the combined cadence admission check. Mixed mode does not
multiply sockets, jobs or journal queues. A single remaining board provides
continuity of functions but no remaining hardware redundancy.

Failover is bounded by the 90–120 second contact lease and the next healthy
check-in, plus any required custody drain. Software simulation covers placement,
all 31 nonempty role combinations, lease/reboot fences and handoff. Physical
ESP32-S3 resource, network and power-loss qualification remains required.
