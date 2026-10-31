# Channel Bus push delivery v1

Shared contract for ESP32 relay task c81c7fff-7fef-4eec-ae35-4f384c8f5052,
credential task a54ffdfc-95bf-4d4c-978a-90aa440e4a81, bot participants and
Android receivers. No third-party broker or inbound device listener is needed.

The relay polls its own event attachments on `capabilities.events`, `bpmn.events`,
`scheduler.events` and `relay.subscriptions`. It does not compete for commands.
Watched channels with enabled subscriptions and the subscription control channel
are checked every 5 seconds; unused channels every 30 seconds. These are target
cadences, not a hard latency guarantee: one shared TLS slot, service latency,
maintenance and retries can delay a notification. A healthy webhook should be
tested against the approximately 10-second acceptance target on the actual node.

## Subscription and private setup

The private pairing policy may include `relay` with `schema_version:1`,
`relay_id`, HTTPS `url` for `/v1/relay/call`, `credential_ref`, optional empty
`site_credential_ref`, `allowed_origins`, `webhook_credential_refs`, and `subscriptions`. There are at most
four allowed origins and four subscriptions. Exact HTTPS origins constrain
destinations; redirects are never followed. Secrets are in the pairing file's
`credentials` map, stored in the device's existing transactional nonvolatile
app-data store. They are not compiled into the public binary or sent on the bus.

Each subscription has `id`, `channel`, `match:{types,filters}`, `url`,
`auth:{header,credential_ref}`, `signing_credential_ref`, and `enabled`.
`types` has up to eight exact event types or `*`. Up to four AND filters use
`{path,op,value}`, where path is a dotted payload field and op is `eq` or
`prefix`, comparing strings. Missing fields do not match. The polling projection
is bounded to eight distinct paths across the entire table.

Grok Worker 1's initial subscription uses channel `capabilities.events`, types
`decision.response_recorded` and `decision.accepted`, and a prefix filter on
`source.business_key` with value `grok-worker-1:`. Its routine URL, secret header
value and signing key must be supplied by Grok Worker 1; none is invented.
The same format supports a BPMN step completion event and a process correlation
filter for another attached agent.

Publish an event of type `relay.subscriptions` version 1 on that channel with
`{relay_id,revision,subscriptions}` to replace the table. Revisions increase
monotonically. Only credential references on the explicit `webhook_credential_refs` allowlist, already held on the device, and origins
admitted through its private pairing are accepted. Secret values never belong
in an event. Bad updates are skipped with a health error; unrelated relay IDs
are ignored. Existing deliveries retain their selected subscription snapshot.
Use the captive portal and an authenticated updated private pairing to add or
rotate secrets/origins. Pending delivery custody must finish first.

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
signing key. Receivers verify the signature in constant time, accept a bounded
timestamp skew (recommended five minutes), and durably deduplicate by relay,
subscription and message ID before returning 2xx. Persist the received event or
its effect before returning success. The signature timestamp may change on retry;
the message ID and delivery body remain stable. Keep deduplication receipts as
long as replay can occur; bus history is retained indefinitely in v1.

## Durability and failure handling

Delivery is **at least once**. A power loss after webhook acceptance and before
the local completion commit can repeat a POST with the same key. Exactly-once
effects require receiver deduplication; a sender alone cannot promise them.
The durable state includes the full compact event, selected subscription
snapshots, completed target index, attempt count, retry time, and channel cursor.
No matching event is acknowledged before all selected receivers return 2xx or
the terminal failure is durably published to the bus dead-letter channel.
Unmatched events are deliberately acknowledged after filtering.

Retries follow 5 seconds, 30 seconds, 2 minutes and 10 minutes, for five total
attempts. Non-2xx, TLS/network failures and uncertain outcomes all retry the
same identity. After the fifth failure, publish `relay.deadletter` version 1
with relay/subscription/message/channel/sequence, attempts and a fixed reason;
omit URLs, secrets and response bodies. Retry that publication idempotently
until confirmed before advancing the event. Delivery is serial and bounded to
one event in flight; a failing recipient applies backpressure to further events.

Publish `relay.health` version 1 every three minutes, including during a webhook
backoff: relay ID, uptime, last acknowledged sequence per channel, queue depth,
subscription count and a bounded error code. Heartbeats are best effort and
never claim hardware qualification or guarantee agent execution.

## Scoped bus credential

`POST /v1/relay/call` uses `Authorization: Bearer <relay token>` and protocol
`relay/1`. Owner-deployed `RELAY_BINDINGS` entries contain relay ID, SHA256 of
the token, enabled flag, expiry, and exact attachment IDs/channels. Raw owner
OAuth and service-wide tokens are never provisioned to the node.

Operations are `configuration`, `receive` (one event, requested field projections),
`ack` (event cursor), and `publish`. Receive/ack are restricted to those exact
attachments and their current channel identity. Publish is restricted to
`relay.health` and `relay.deadletter` with matching relay ID. The route cannot
claim command work, administer channels/attachments, invoke connected tools,
describe unrelated messages, or publish subscription changes. Owner tools create
the attachments and register immutable event contracts before provisioning.

## Maintenance budget accompanying this release

The Scheduler's transaction callback now freezes its timestamp before replaying
lazy reads. Its previous changing due-query key could prevent any progress on a
remote database. Processing begins after admission; receipts expose admission
and processing time separately. The node requests one quantum with an 8-second
processing budget, a 12-second transport timeout and a 60-second cadence.
The server policy is separately capped at 8 seconds for that binding. Other
service bindings retain their own budgets. These bounds preserve time for bus
polling and control contact; they do not certify physical timing.
