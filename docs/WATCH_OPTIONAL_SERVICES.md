# Qualified optional-service startup

The shared portable adapter keeps the original Contexts, BLE broadcast and
foreground alarm feature clients compiled and keeps their capability grants.
It may continue when an optional acquire returns false only if the returned
capability is empty and unchanged in size and a runtime health probe succeeds.
The service API remains null, a diagnostic reports it unavailable, and dependent
controls cannot claim success. Quick Controls retains its Contexts tile with
its existing unavailable presentation and does not write an enable preference
for an absent service. Alarm-related preferences remain ordinary preferences.

Successful provider acquisition still validates the complete API prefix and
all callbacks the client uses. Malformed acquired service APIs fail startup.
False acquisition with a partial grant, unhealthy authority, retained storage,
CONTEXT errors or refused release is not absence. These cases terminally retain
the invocation, including native/async frame state. There is no further provider
I/O, grant release, freeing, handoff or reentry. An older/refusing runtime fence
uses a no-I/O terminal spin rather than an ordinary provider-polling yield.

A synchronous checked forwarding table preserves KV CONTEXT errors through the
existing bool preference helpers; it delegates only to the real validated
provider and suppresses further callbacks after the first revoked access.
It does not register a provider or supply a missing service.

Display, touch, required Settings storage/RTC policy, deployment grants and
feature flags are unchanged. No board names, hardware identifiers or fabricated
capabilities are part of the availability decision.

## Qualification

Run:

    python3 scripts/test_optional_service_adapter.py

The script uses the actual adapter and Settings source with Contexts, broadcast,
alarm, Quick Controls, radio controls and low-battery support. Its matrix covers
240x240 synchronous and 410x502 native-height asynchronous profiles, each normal
and ASan/UBSan. It records results in build/optional-services/result.json.

The 276 cases include:

- All eight present/missing combinations, three normal entries each, and opening
  and closing Quick Controls without removing its UI.
- Wrong-version/truncated service tables, missing callbacks, false acquire with
  API or token custody, empty false acquire on unhealthy runtime, and malformed
  successful token-only acquire.
- Refused release, including a provider that clears the grant before returning
  false, and post-fence repeated init/poll/paint/input/fini calls.
- Fingerprint/rule storage RETAINED and CONTEXT; partial/unhealthy acquisition;
  optional-client KV partial/unhealthy acquisition, release refusal and read or
  write CONTEXT.
- Post-open periodic checkpoint, explicit rule save, fini, alarm due, and
  foreground-error cleanup failures. Async post-open cases hold an ACTIVE
  presentation token and prove there are no later present-status calls.

The runtime-provider implementations in these tests are deterministic fixtures;
passing does not establish physical hardware operation. LeakSanitizer is disabled
only when the host explicitly reports its ptrace incompatibility. Address and
undefined-behavior sanitizers remain enabled.

Additional focused regressions: test_context_adapter.py,
test_broadcast_adapter.py, test_portable_alarm.py, and normal/sanitized
context_rule_storage_test.c. Broadcast client assertions now require terminal
behavior after health/release failure; the previous retry expectation was unsafe.

## Existing baseline failures

The complete Contexts client suite is not green. Before this patch, both failures
below were reproduced using a clean archive of commit
070773a0d4c24dda99d2424149755c0939a1d854. The original output is retained under
build/optional-services/evidence/optional-context-baseline.log and
build/optional-services/evidence/optional-fp-baseline.log.

Reproduction (repeat the compile/run pair for each named test):

    tmp=$(mktemp -d)
    git archive 070773a0d4c24dda99d2424149755c0939a1d854 lib/PortableApps/include test/native_apps/context_client_test.c test/native_apps/context_fingerprint_client_test.c | tar -x -C "$tmp"
    cc -std=c11 -Wall -Wextra -Werror -I"$tmp/lib/PortableApps/include" "$tmp/test/native_apps/context_client_test.c" -o "$tmp/client"
    "$tmp/client"
    cc -std=c11 -Wall -Wextra -Werror -I"$tmp/lib/PortableApps/include" "$tmp/test/native_apps/context_fingerprint_client_test.c" -o "$tmp/fp"
    "$tmp/fp"

- context_client_test.c:93 aborts at
  `portable_contexts_step(&c,true,true)&&!last_policy.radio_allowed` after an
  external preference change. Its elapsed-time reload expectation differs from
  the existing invalidation-based client cache.
- context_fingerprint_client_test.c:49 aborts at
  `portable_contexts_open(&c,&runtime)&&imports==2&&!writes`. The baseline fixture
  recognizes storage.app-data while the existing client requests
  storage.shared-data; its runtime also lacks a health callback.

These are not asserted as newly passing by this change. No publication, device
image, flash, or physical-device qualification is included in this receipt.

## Separately gated local-navigation custody query

`PORTABLE_NAVIGATION_RETENTION` together with `PORTABLE_INPUT_NAVIGATION_LOCAL`
requires the bridge's no-I/O `portable_input_navigation_retained()` query. The
adapter checks it around open, foreground, reset, poll and close. A latched bridge
failure enters the same terminal invocation fence before subsequent I/O or frees,
including Settings' later RTC-grant cleanup. Older bridge recipes do not define
this opt-in and do not gain an unresolved symbol.

`python3 scripts/test_navigation_retention_adapter.py` covers 20 normal and
ASan/UBSan actual-adapter boundary-spy cases, including healthy repeated entry.
These spies do not replace the real bridge. The opt-in deployment profile remains
pending separate qualification linking the new generic bridge and actual adapter.
