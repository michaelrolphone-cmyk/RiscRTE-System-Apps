# Unified Channel Bus push delivery — firmware 1.5.0

Shared contract for ESP32 relay task c81c7fff-7fef-4eec-ae35-4f384c8f5052,
credential task a54ffdfc-95bf-4d4c-978a-90aa440e4a81, bot participants and
Android receivers. No third-party broker or inbound device listener is needed.

The relay polls its own event attachments on `capabilities.events`, `bpmn.events`,
`scheduler.events`, `rhizome.events` and `relay.enroll`. It does not compete for commands.
Watched channels with enabled subscriptions and the enrollment control channel
are checked every second; unused channels every 30 seconds. These are target
cadences, not a hard latency guarantee: one shared TLS slot, service latency,
maintenance and retries can delay a notification. A healthy webhook should be
tested against the approximately 10-second acceptance target on the actual node.

## Register, rotate and revoke through the bus

Firmware v1.4.0 uses `relay-enroll/1`. No webhook URL, origin, secret or subscription
needs to be entered into the captive portal. Pairing supplies only the relay ID,
HTTPS bus URL and scoped bus credential. Legacy pairing fields are optional for
migration. The old `relay.subscriptions` replacement command is drained without
applying it; it cannot bypass approval.

1. Read the relay's newest `relay.identity` v1 event. Verify `relay_id`,
   `installation_id`, `key_id` and `enroll_public_key` against the authenticated
   Capabilities participant's `labels.relay_enrollment`. Do not trust a public
   bus event alone. The key is persistent X25519; `key_id` is lowercase SHA256
   of the 64-character lowercase public-key hex string.
2. Generate a new enrollment ID and ephemeral key. Seal the private webhook
   fields locally using `scripts/relay-enroll.mjs`. It reads private input on
   stdin and writes only the public request plus ciphertext to stdout.
3. Publish that output as type `relay.enroll`, version 1, channel `relay.enroll`.
   Use the enrollment ID as a stable publisher idempotency key. Retry the same
   message identity after an uncertain publish; do not reseal with an old ID.
4. Michael receives one Decisions question showing the subscriber, HTTPS origin,
   channel, types, filter, operation and enrollment digest. The question requires
   an authenticated phone interaction and expires after 24 hours.
5. Wait for `relay.enroll.result` v1 with the same relay/enrollment ID. Status is
   `applied`, `rejected`, `expired` or `invalid`, with subscription ID, committed
   revision and a redacted error code. Only `applied` confirms installation.

The public request has `relay_id`, `enrollment_id`, `subscriber_id`, `requested_by`,
`operation` (`add`, `rotate` or `revoke`; omitted means `add`), exact HTTPS `origin`,
`subscription:{id,channel,match:{types,filters},enabled}` and `sealed`.
The sealed object contains `alg:"x25519-hkdf-sha256-chacha20poly1305"`, `key_id`,
`ephemeral_public_key`, `nonce` and `ciphertext`; all bytes are lowercase hex.
The nonce is 12 bytes, the ephemeral X25519 public key 32 bytes. Ciphertext ends
with the 16-byte Poly1305 tag. The helper uses Node/OpenSSL; firmware uses the
unmodified pinned Monocypher 4.0.2 implementation.

The helper's private input is `{identity,request,secret}`. For add/rotate, `secret`
is `{url_path,auth_header_name,auth_header_value,signing_key}`. Paths start with
one `/`; no origin, fragment or redirect is allowed. For revoke, seal `{}` and
name the existing subscription. Every rotation and revocation needs a new
approval. Never publish the helper input or its private file.

AEAD associated data is canonical JSON with sorted keys:
`{enrollment_id,key_id,operation,origin,relay_id,requested_by,subscriber_id,subscription_digest}`.
`subscription_digest` is lowercase SHA256 of canonical public subscription JSON.
The X25519 shared secret is fed to HKDF-SHA256 with salt SHA256(AAD), info UTF-8
`relay-enroll/1`, and output length 32. ChaCha20-Poly1305 uses the AAD verbatim.

The authenticated relay route fetches the original bus message, creates the
idempotent owner question and validates the accepted answer's question digest,
authorization scope, owner, participant and interaction assurance. The node checks
the request digest before decrypting. Origins, local secret references, subscription,
revision and completion receipt are saved in one existing journal transaction.
A reboot before result publication retries the receipt without applying twice.
The server retains enrollment-ID and nonce replay records. Approval accepted before
the deadline remains recoverable if the node reconnects after that deadline.

Bounds: four pending enrollments, eight subscriptions, eight distinct origins,
eight projected filter paths, eight event types and four filters per subscription.
Requests are at most 6144 UTF-8 bytes; private sealed data fits within 1536 bytes
including the tag. Filters use `eq` or `prefix` on strings; missing fields do not
match. Enrollment control calls have a 25-second transport bound; ordinary relay
polls and webhook sends retain their five-second bound.

Grok Worker 1 registers `grok-worker-1-decisions` on `capabilities.events`, with
`decision.response_recorded` and `decision.accepted`, and a prefix filter on
`source.business_key` equal to `grok-worker-1:`. Grok supplies its actual private
path, `Authorization` header value and signing key through the sealing helper.
A software test callback is not evidence that the live Grok webhook is active.

## HTTPS delivery

POST UTF-8 JSON with:

```json
{"protocol_version":"relay-delivery/1","relay_id":"node-id","subscription_id":"grok-worker-1","channel":"capabilities.events","seq":42,"message_id":"bus-message-id","type":"decision.accepted","correlation":"process-key","created":1793400000000,"summary":"decision.accepted on capabilities.events","payload":{"example":true}}
```

Payloads over 8192 UTF-8 bytes are omitted and `payload_omitted:true` is included.
The receiver retrieves the full message using `bus_describe(message_id)` with its
own authorized bus credentials. Summaries contain no line breaks. Headers are:

- `Content-Type: application/json`
- `Idempotency-Key: <message_id>`
- `X-Relay-Timestamp: <UTC seconds>`
- `X-Relay-Signature: sha256=<lowercase HMAC-SHA256 hex>`
- The subscription's secret header, copied exactly from device-held credentials.

The HMAC signs the exact bytes `timestamp + "." + HTTP body`, with the subscription
signing key. Receivers using the optional generic signature verify it in constant time, accept a bounded
timestamp skew (recommended five minutes), and durably deduplicate by relay,
subscription and message ID before returning 2xx. Persist the received event or
its effect before returning success. The signature timestamp may change on retry;
the message ID and delivery body remain stable. Keep deduplication receipts as
long as replay can occur; bus history is retained indefinitely in v1.

## Durability and failure handling

Delivery is **at least once**. A power loss after webhook acceptance and before
the local completion commit can repeat a POST with the same key. Exactly-once
effects require receiver deduplication; a sender alone cannot promise them.
Legacy custody retains its compact event and target snapshots until drained.
New custody retains one event identity and canonical digest per subscription,
a selected subscription snapshot, attempt count, retry time and attachment cursor.
The payload is reloaded from that unacknowledged Bus attachment and its digest
verified before each send. This keeps eight large events out of the device journal.
No matching event is acknowledged before all selected receivers return 2xx or
the terminal failure is durably published to the bus dead-letter channel.
Unmatched events are acknowledged after filtering. Each receive examines up to
25 events, returning the first match without advancing past it.

Retries follow 5 seconds, 30 seconds, 2 minutes and 10 minutes, for five total
attempts. Non-2xx, TLS/network failures and uncertain outcomes all retry the
same identity. After the fifth failure, publish `relay.deadletter` version 2
with relay/subscription/message/channel/sequence, attempts and a short error code;
omit URLs, secrets and response bodies. Retry that publication idempotently
until confirmed before advancing the event. There is one TLS connection and one shared delivery transition loop, with an
independent persisted retry slot and Bus attachment per subscription. Backoff
on one destination does not block others. A subscription retains global Bus sequence ordering across its channels.
The Bus selects the earliest retained next event and bounds filter scans before
an earlier event on another channel; the device acknowledges that selected attachment.

Publish `relay.health` version 2 every three minutes, including during a webhook
backoff: relay ID, uptime, last acknowledged sequence per channel, queue depth,
subscription count, per-subscription delivery sequence/failure count/error code
and a bounded global error code. Heartbeats are best effort and
never claim hardware qualification or guarantee agent execution.

## Scoped bus credential

`POST /v1/relay/call` uses `Authorization: Bearer <relay token>` and protocol
`relay/1`. Owner-deployed `RELAY_BINDINGS` entries contain relay ID, SHA256 of
the token, enabled flag, expiry, and exact attachment IDs/channels. A separate
`RELAY_ENROLLMENT_BINDINGS` setting binds each relay to its Capabilities participant
and its single enrollment attachment without replacing the existing credential.
Raw owner
OAuth and service-wide tokens are never provisioned to the node.

Operations are `configuration`, `receive` (one event, requested field projections),
`ack` (event cursor), and `publish`. Receive/ack are restricted to those exact
attachments and their current channel identity. Publish is restricted to
`relay.health`, `relay.delivery` and `relay.deadletter` with matching relay ID. The route cannot
claim command work, administer channels/attachments, invoke arbitrary connected tools,
describe unrelated messages, or publish subscription changes. Owner tools create
the attachments and register immutable event contracts before provisioning.
The additional `identity`, `enrollment_review` and `enrollment_complete` operations
are confined to the bound participant, original enrollment message and its exact
owner decision. The device never receives the service bridge credential.

## Maintenance budget accompanying this release

The Scheduler's transaction callback now freezes its timestamp before replaying
lazy reads. Its previous changing due-query key could prevent any progress on a
remote database. Processing begins after admission; receipts expose admission
and processing time separately. The node requests one quantum with an 8-second
processing budget, a 12-second transport timeout and a 60-second cadence.
The server policy is separately capped at 8 seconds for that binding. Other
service bindings retain their own budgets. These bounds preserve time for bus
polling and control contact; they do not certify physical timing.


## ChatGPT MCP Events and v1.5.0 migration

The same firmware subscription table accepts `destination.kind=webhook|mcp_events`
and `channels` (up to four). Legacy `channel` records remain valid. The native
MCP endpoint exposes `events/list`, `events/subscribe` and `events/unsubscribe`.
Its `relay.delivery` event accepts the same types and path/op/value filters.
Authenticated principal, canonical arguments, event name and callback URL bind
the stable subscription ID. The front door writes sealed enrollment receipts
in the existing Bus enrollment table; it has no HTTP delivery worker or separate
subscription authority. The device journal is the installed subscription store.

`relay-enroll/2` extends the existing seal: include `protocol_version` in canonical
AAD and use HKDF info `relay-enroll/2`. The sealed payload is
`{url_path,headers?,signing?:{alg,key}}`. MCP uses `standard-webhooks` and a
`whsec_` base64 key decoding to 24–64 bytes. Public fields include destination,
principal hash, monotonic generation, expiry and routine binding. V1 ciphertext,
identity keys and current enrollments migrate unchanged. New destinations each
require one owner Decisions approval. Authenticated renewal/rotation/revocation
of an installed MCP destination reuses that decision through a checked enrollment
parent; arbitrary bus publishers cannot grant this reuse.

The ESP32 performs a signed, random, expiring verification challenge and checks
the echo in constant time. This carries no application event and can run while
the owner decision is pending. Only a successful device verification lets the
front door return a subscription ID; only approval plus atomic installation
permits event delivery. Rejected/expired questions never activate a subscription.
A subscription request may return callback_verification_pending if the device
cannot complete verification within 25 seconds; retry identical arguments.

Every application delivery contains the same `relay-delivery/1` object and
`agent-invocation/1` object. The invocation ID is lowercase SHA256 over UTF-8
`subscription_id|routine_id@routine_version|channel|message_id`.
The optional fanout group is preserved. Idempotency-Key remains message_id;
receivers deduplicate (subscription_id,message_id). Existing notification-only
subscriptions default to relay-notice@1 and have no execution assignment.
HTTP acceptance never settles a Capabilities assignment or reports run completion;
business effects still require an authorized fence and agent-outcome/1 receipt.

The MCP adapter wraps that identical delivery as the native event's data:
`{eventId,name,timestamp,cursor:null,data}`. It signs the exact transmitted bytes
using Standard Webhooks: HMAC-SHA256(decoded secret,id+'.'+seconds+'.'+body),
base64 with a v1, prefix. It sends webhook-id, webhook-timestamp,
webhook-signature and X-MCP-Subscription-Id. Rotation sends both signatures for
five minutes. 410 and 413 are terminal for that attempt; 410 also disables the
subscription. Generic/Grok delivery keeps the relay body unchanged and supplies
the stored Authorization header. Grok need not check a signature. A 2xx is trigger
acceptance, never evidence that an agent ran to completion.

Native RISC_HTTP_POST_PUBLIC is additive to the existing HTTP v2 ABI. It resolves
DNS, denies private/reserved IPv4 ranges, connects only to that checked address,
retains the original TLS hostname and does not redirect. IPv6-only callbacks are
currently unsupported and fail closed. Old runtimes reject the new method;
upgrade the paired native firmware and application store together.

Each HTTP attempt durably stages a redacted relay.delivery v1 receipt before
advancing. It contains only relay/subscription/message/invocation identifiers,
attempt, HTTP status and observation time. URLs, bodies and credentials are absent.
The immutable health v1 schema capped queue_depth at one and subscriptions at
four, and deadletter v1 required exactly five attempts. Health/deadletter v2
extend those contracts; old v1 readers and retained events remain untouched.
The delivery body protocol stays relay-delivery/1.

Upgrade using the preserving phone repair bundle. Drain any legacy pending
request on its original attachment before enabling independent cursors. Preserve
identity, enrollment queue, sealed requests, secrets, Wi-Fi settings and participant
credentials; change only the pinned provider cohort. Never use the first-install
full-flash image on an enrolled device. Historical enrollment seq 5163 is untouched.

Host evidence covers 301-event independent cursor catch-up, failure isolation
across reboot, signature vectors, owner approval, wrong challenge denial,
legacy custody migration and terminal 410/413. These are software tests, not
measurements of live device latency or a claim of native ChatGPT activation.

The preserving installer additionally accepts the exact published 22,640-byte
v1.4.2 bootloader, SHA256
`6ef091e6fa1c102ff4449b47677d52fb13ea4078b1888b48bb0a0ed8c24bc3fb`.
Binary comparison confirms identical code; only the build timestamp, checksum
and image digest differ from the new build. The installer leaves it untouched.
