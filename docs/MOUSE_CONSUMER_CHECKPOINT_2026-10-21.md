# Mouse consumer unvalidated source checkpoint

Development stopped at 2026-10-21 13:55 UTC to capture existing work in GitHub.
This checkpoint is not delivered, completed, qualified, or task progress until
its exact commit is verified in GitHub. Publication is coordinated separately;
no push, PR creation, merge, release, product selection or hardware action was
performed by this source work.

## Repository and dependencies

- Target repository: `michaelrolphone-cmyk/RiscRTE-System-Apps`.
- Working branch: `work/mouse-copied-consumer`.
- Base: `f9d55dbeb941ba40fb965f53172ce3e1b85af376`, the existing serial-client
  candidate, whose own parent is `0651a55513b0aa0dff1b6a9072303def08a97378`.
  Whether the serial-client base is in GitHub must be verified before publishing
  this branch or choosing a PR base. Do not imply it is accepted upstream.
- Native Runtime source: `6a3b3d46f5b2857eacc6e9863799dec1f466b036`.
- Recovered Reader provider source: `c334919a05d393b7b3a536fceb117cfbb573a4ef`;
  evidence head: `59177418ea746c7605b7b5a5aec501e70bdae80f` in existing draft
  [PR468](https://github.com/michaelrolphone-cmyk/T5S3-Reader/pull/468), stacked
  on open draft [PR467](https://github.com/michaelrolphone-cmyk/T5S3-Reader/pull/467).
  Neither PR was changed. No abandonment or ownership transfer is inferred.

The local recovered Reader input is a materialized source tree, not a Git
checkout. The draft qualification runner deliberately expects a clean checkout
at the exact evidence head, and cannot pass that check on this materialization.
Restore or obtain the exact public checkout before running it. The immutable
source custody was supplied by the prior recovery report; this checkpoint's
header copies were compared directly to those recovered bytes, not freshly
recovered from Git during this slice.

## Captured source

- `PortableMouseConsumer.h/.c`: opt-in app-local native generic lease consumer,
  not a new mouse provider or product binding.
- Four byte-identical USB ABI headers and provenance. Existing serial/provider
  and stream-header dependencies are unchanged. T5 legacy lease getter unused.
- Fault-injection unit fixture; draft documentation and qualification runner.
- Four `test/native_apps/mouse_runtime_*` draft integration fixture files.

The helper uses Runtime's provider-free stream-client invocation identity,
without stream operations, before borrowed provider access. It separates
copied-event/focus work from synchronous acquire/release graph lifecycle. It
never directly calls provider poll/yield/raw HID. Initial snapshot, per-session
held state, relative-event sequence checks, GAP recovery and terminal cleanup
custody are implemented but not fully qualified.

## Executed check

At approximately 13:51 UTC, this focused command completed with exit code zero:

```sh
cc -std=c11 -Wall -Wextra -Werror -g -DPORTABLE_USB_MOUSE \
  -Ilib/PortableApps/include lib/PortableApps/src/PortableMouseConsumer.c \
  test/native_apps/portable_mouse_consumer_test.c -o /tmp/mouse-consumer-unit
/tmp/mouse-consumer-unit
```

Compiler: `cc (Debian 14.2.0-19) 14.2.0`.
Output: `mouse consumer: copied events, recovery, focus and custody PASS`.
This one body used the existing System SDK prefix. No full-suite, sanitizer,
current-native-SDK, target-build or physical test result is established here.

A contract-only independent review established that pinned Runtime's
`stream_client` getter is a suitable provider-free invocation identity check,
that acquire services providers, and that failed graph release permanently ends
borrowed-pointer usability. A separate final source review was stopped before
completion. It reported no concrete blocker in inspected portions, not an
approval or completed qualification.

## Pending work and explicit limits

1. Verify and publish this exact checkpoint plus its required base dependency
   to GitHub, with an unvalidated draft PR. Do not call source delivery complete
   until the remote SHA and PR are verified.
2. Complete `scripts/run_mouse_runtime_integration.sh` (currently absent),
   compile/review the four draft fixtures, and run real recovered provider plus
   native Runtime graph tests. Intended eager/demand distinction: eager Runtime
   retains a boot grant, so final graph cleanup is not helper-release cleanup.
3. Run the complete draft qualification runner on exact clean upstream Git
   checkouts normally and with ASan/UBSan. Record actual hashes/logs. No current
   end-to-end runner success is claimed.
4. Complete final independent review. Visible untested cases include a false
   retain_invocation result, malformed stream-client tag version/size, and a
   CONNECTED record at maximum device capacity.
5. Independently qualify the producer executor, serialization, manifest/grant
   admission and intended product input routing. No cursor, scaling or UI focus
   policy is selected. Existing mouse poll has 20 ms report work, a 1 ms yield,
   and separately bounded 100 ms raw HID calls; it is not a UI callback.
6. Preserve lower-host quarantine/checked-host custody and X4 VBUS/role/source
   route/current-limit/electrical proof. Raw HID's void host release does not
   certify physical quiescence. No hardware acceptance is requested.

All Reader/provider source, accepted binary bytes, existing application
manifests, product pins and other owners' active branches remain unchanged.
