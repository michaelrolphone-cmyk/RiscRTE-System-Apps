# Opt-in sparse desk-clock adapter

`PORTABLE_DESK_CLOCK_SPARSE_START` changes only the selected desk-clock
application's adapter lifecycle. It requires `PORTABLE_DESK_CLOCK`,
`PORTABLE_ALARM_CLIENT` and `PORTABLE_APP_SLEEP_LOCAL`. No manifest, application,
Settings/timezone, Runtime or product profile is changed by this slice. Builds
without the new flag keep the existing eager lifecycle.

## Application integration

1. `app_module_init` validates/copies the Runtime table and initializes software
   state. It acquires no provider, reads no health/preferences and starts no I/O.
2. In `app_main`, classify the wake using the deployment's existing authority.
   Call `portable_desk_adapter_start(PORTABLE_DESK_START_TIMER)` for admitted
   timer work, or `PORTABLE_DESK_START_FOREGROUND` for ordinary foreground.
3. TIMER binds `display.output` and `alarm.service`. It may allocate the normal
   alarm-frame copy and MONO damage/history cache. The complete alarm-service
   dependency closure remains active for Alarms, Countdown and Points. This is
   not a display-only or arbitrary hardware-inactivity claim.
4. TIMER does not open touch/navigation/battery, directly bind RTC, load
   QuickActions preferences or restore radios. Its paper-presentation getter
   returns the existing rendering table without the foreground RTC bind.
   Present/wait and defensive app polling never poll absent subscriptions.
   The Clock owns timer alarm reconciliation and promotion decisions.
5. TIMER sleep bypasses `portable_desk_clock_mode` and its preferences lookup,
   and does not prepare/reset/close/reopen absent foreground resources. It calls
   the selected local alarm-aware sleep hook with display, alarm service and
   NULL battery. Ordinary refusal leaves TIMER active.
6. Before upgrading, the parent calls Runtime0.1.50 provider promotion under its
   admitted default-app authority. Pass the actual result to
   `portable_desk_adapter_upgrade(result)`. Only OK=0 or ALREADY_READY=1 permits
   upgrade; RETAINED=-4 fences immediately. No promotion grant is acquired by
   the adapter. Result values are a coordinated application contract, not
   independent proof of promotion or provider inactivity. Runtime FAILED may
   preserve partially promoted boot pins: the caller must retry or exit, and
   must not interpret adapter TIMER state as permission to resume sparse sleep
   after a promotion attempt.
7. Upgrade requires a settled display and no outstanding app frame. It keeps
   the same display grant, mapping and damage/history cache, then opens input,
   battery, preferences and radios. It never drops the display dependency
   reference during the transition. For a normal foreground boot, deployment
   orchestration must establish its boot pins before transient recovery grants
   can lose their last dependency reference.
8. Both start/upgrade return 1 for success, 0 for refusal or clean failure, and
   -2 for uncertain custody. A same-mode repeat is idempotent. Foreground start
   cannot upgrade TIMER implicitly, and foreground cannot downgrade to TIMER.
   Invalid/precondition refusals perform no acquisition. A clean startup error
   permits finalization only. Retained paths forbid all further normal I/O,
   retry, cleanup, release and finalization.

`portable_desk_adapter_timer_only()` is a pure state query for the selected
X4 app-local sleep hook. That hook may rely on its unopened foreground set only
within this coordinated build. The query does not attest that arbitrary
providers, clients or hardware are inactive, and does not authorize skipping
required holds on the still-live display/alarm closure.

## Ownership and input boundaries

Any Runtime acquisition failure is conservatively retained, including false
with an empty output or a partially populated output. Absence of an output
cannot prove native graph rollback. The local guarded Runtime table also
fences release and health failures. The selected sleep hook must check the
retained/readiness state after Runtime operations and must not continue I/O
when a failure has latched retention. Radio control table copies prevent
QuickActions from operating earlier radio grants after a later KV acquisition
fails. Runtime release still receives its original table identity.

Subscription/foreground-claim/reset failures are retained. Checked finalization
stops at its first uncertain operation; repeated fini and fini before start do
no I/O. Failed display history seeding/presentation retains the frame descriptor
and leaves already-copied drawing callbacks inert. Owned grants/cache remain
pinned rather than being released or freed under uncertain custody.

The first held touch/Home and navigation sequence after upgrade is discarded
through a fresh neutral snapshot, including drag and replay state. The next
fresh contact is eligible normally. No old contact/action is replayed into
ordinary Clock or QuickActions.

## Software verification

Run `scripts/test_sparse_clock_adapter.py --sdk <canonical-driver-sdk>
--xtensa-cc <pinned-8.4.0-esp-2021r2-patch5-gcc>`.

The fixtures compile the real production adapter and QuickActions sources over
deterministic host provider tables. They cover software-only initialization,
finalization without start, sync/async timer presentation, no absent-input
polling, timer sleep refusal/retention, mapping/history-preserving upgrade,
full startup, repeated/invalid calls, held/replayed-input neutrality, nine
acquisition failure positions with empty and partial outputs, upgrade failures,
allocation fallback, failed subscribe/claim/reset, startup/finalization release
failures, retained history seeding, and no I/O after retained ownership.

The runner executes normal and ASan/UBSan variants with
`ASAN_OPTIONS=detect_leaks=0`, links sparse adapter harnesses with the pinned
Xtensa compiler, and compares flag-off Watch/paper preprocessed paths and
linked adapter harness ELF bytes against the exact integration base.

These are adapter fixtures, not real-provider execution or Runtime promotion
tests. No product BIN, physical power, deep-sleep wake, display timing or hardware
qualification is implied. Application/profile integration and actual provider
promotion verification are separate work.

### Validation record (2026-10-07)

The executor was replaced during implementation. Work resumed on public base
`3bcc9b3accbd8169f0441a6874114127a69507d0`; its adapter and private-header blobs
are identical to the originally assigned `c09342c` inputs:

- adapter: `c8a1e39ae343ed63e96d5205e1b6b56667b934bc`
- header: `21acd0a45336d34b13d123eab332e998e0c10774`

The reconstructed, corrected sources passed 58 normal and 58 ASan/UBSan cases,
including canonical provider-table preservation outside QuickActions, failure
of health during input service, uncertain foreground sleep unsubscribe, and
unconfirmed Wi-Fi/BLE off status.
Three sparse feature configurations linked and passed the production ELF
structural validator and exact imports/exports checks with Xtensa
`8.4.0 (crosstool-NG esp-2021r2-patch5)`.

Flag-off Watch/paper preprocessed C tokens and linked adapter harness bytes
match the recovered base. The harness SHA-256 values are respectively:

- `b527d58d451fd7aa99941f981cb0105ab009b7773429e5e75413b66d43b88e6a`
- `b14d984dd8feb8c069d510ee68b141cf5efa338f42b820950ba85827c626efd8`

These are harness ELFs, not delivered Watch/X4 firmware images. Deployment
build provenance must include `lib/PortableApps/src/sparse_clock_adapter.inc`
as well as `adapter.c` when enabling this flag.

The existing `test_paper_desk_clock.py` regression also passed on the final
adapter opt-out path: normal/sanitized manual and timer boot, refusal/retention,
old-image pixels, deferred radios, and six faces over 62 fresh-process cycles.
