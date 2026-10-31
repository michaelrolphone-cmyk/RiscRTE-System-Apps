# ESP32-S3 infrastructure conformance record

Specification revision: `f0e52f9fb704bd984a1b7ed8d0bdc9c74b2596a875d58ea74b7354f2c6667ac2` (sections 1–40 and S3-01–48). Source commit identities and exact file identities are in the release `flash-manifest.json`. This record distinguishes software evidence from live-service and physical qualification. A host simulation of a cut is not evidence of a physical power cut.

## Evidence and status rules

The release handoff must state four independent values: `implementation_complete`, `artifacts_delivered`, `live_integration_verified`, and `hardware_qualified`. The first two require completed source/build and durable upload/readback respectively; this source document does not assert that a subsequent publication occurred. Live credentialed verification and hardware qualification are pending. A release must not claim implementation complete while a required available-environment check is failing.

| Evidence key | Execution layer and observed result | Relevant source |
| --- | --- | --- |
| NATIVE | ESP-IDF target build succeeded; `riscrte_native.bin` was `0x17ab60` bytes in the recorded build; bootloader `0x5870`; exact release sizes come from manifest | Runtime headless profile; System Apps `profiles/esp32s3-infrastructure/native.cmake` |
| ELF | Xtensa linking/import validation passed for `default.elf`, `infra-node.elf`, `infra-station.elf`; rebuilt after boot-health correction | `scripts/build_infrastructure.py`, `Apps/infra_agent/main.cpp`, `Services/infra_node/` |
| HTTP | 206 native HTTP parser/transport/lifecycle cases plus v1/time-suffix C ABI and CPU cleanup tests passed | Runtime `sdk/driver/RiscHttpClientV2.h`, `src/ports/esp32s3/NativeHttp.h`, `HttpResponse.h`, `NativeDns.h`; `http-final-test.log` |
| STORAGE | Full-file bounds, namespace/quota/stage recovery, every-chunk faults, uncertain rename, immutable snapshots, restart, async copied writes/10,000 pending polls and provider-bound retained fences passed | Runtime app-data implementation and tests; `runtime-storage-test.log`, `runtime-async-data-test.log`, `runtime-bound-data-test.log` |
| MEMORY | Scratch wipe, shared random/memory admission, ownership and grant revocation tests passed | Runtime memory/random implementation; `runtime-memory-test.log` |
| NODE | Five host suites passed: strict JSON/capacity/coalescing/lease/overload, policy, journal uncertainty/retained fence, transport time, boot-health gating | `test/infra_node/{core,policy,journal,transport,boot_health}.cpp`; `agent-full-test.log` |
| HARNESS | Production Node C++ plus actual service domain modules: PASS, 171 calls, 32 wakes, 8 polls, 3 progress steps, 5 inventory reports, 2 accepted results, 3 stops, 4 reboots, 1 dependency result | `test/infra_node/node.cpp`, `scripts/test_infrastructure_service.mjs`; `agent-full-test.log` / `node-service-test.log` |
| CAP | 57 service tests passed, including bounded sync, claims/effects/results, cancellation, rotation, restores, composition, pools and HTTP/MCP/Bus receipt equivalence | Capabilities `lib/capabilities/`, `tests/`; `capabilities-final-test.log` |
| CAP-M | Two scoped maintenance tests passed; real bounded adapter is also exercised by HARNESS | Capabilities `lib/capabilities/maintenance.mjs`, `tests/maintenance.test.mjs`; `capabilities-maintenance-test.log` |
| SCHED | 31 tests passed after production build, including bounded maintenance, target lease, receipt replay/retention, durable catch-up, real built-worker/D1 tests | Scheduler `lib/maintenance.mjs`, `tests/`; `scheduler-final-test.log` |
| INSTALL | 13 production-installer regression tests passed: selectors, corruption, layout, interruption prefixes, recovery and protection from disabled assertions | `scripts/test_infrastructure_install_recovery.py`; `installer-test.log` |
| PACKAGE | Release packaging must pass SPIFFS exact extraction, pinned LittleFS remount/readback, ELF admission/import checks, manifest hashes/ranges and bundled installer verification | `scripts/package_infrastructure.py`, `provision_infrastructure.py`, `infrastructure_seed.c`, `install_infrastructure.py`; release packaging log |

Counts describe the recorded runs, not independent physical devices. Packaging must be rerun from the final committed tree; the release manifest and packaging log take precedence over intermediate binary sizes. Build/test logs supplied in `evidence/` identify the actual execution and outcomes.

## Sections 1–40: requirement to implementation to evidence

Paths without a repository prefix refer to System Apps. Runtime and service paths refer to the corresponding repositories pinned in the release evidence.

| Section | Implementation | Evidence / remaining boundary |
| --- | --- | --- |
| 1 Outcome/scope | Headless profile, app and `infra.node` service; four baseline providers | NATIVE, ELF, HARNESS |
| 2 Ownership | `Node.cpp` participant adapter, service domain authority, endpoint-scoped policy | NODE, CAP, SCHED |
| 3 Hardware/profile | `native.cmake`, partition table, installer geometry checks | NATIVE, INSTALL; physical module and PSRAM pending |
| 4 Capabilities | `scripts/infrastructure_fixtures.py`, generated catalog and manifests | ELF, HARNESS, CAP |
| 5 Composition | Provider dependency/pin fixtures; Node prepare/start/result paths; service planner | HARNESS dependency result; CAP composition/cancellation tests |
| 6 Decomposition | `Apps/infra_agent/main.cpp`, `Services/infra_node/{service,station,Node}.cpp` | ELF, NODE, HARNESS |
| 7 Capacity | `Core.h`, `Node.cpp` shared inventory and pool accounting; runtime heap snapshots | MEMORY, NODE, HARNESS, CAP; physical headroom pending |
| 8 Availability | Node fresh session/inventory, expiry and withdrawal logic | NODE, HARNESS, CAP |
| 9 Existing runtime extensions | Additive HTTP v2 and existing app-data backend integration | HTTP, STORAGE, NATIVE |
| 10 HTTPS ABI | Runtime `RiscHttpClientV2.h`, `NativeHttp.h`, `HttpResponse.h` | HTTP; real certificate/host endpoint validation pending |
| 11 Cooperative cleanup | HTTP/DNS bounded progress; Node poll state machine; retained barriers | HTTP, STORAGE, NODE |
| 12 TLS/credentials/time | `Transport.h`, scoped policy, native TLS date/host validation and time bootstrap | HTTP, NODE, CAP rotation/auth; live TLS/rotation pending |
| 13 Persistent state | `Journal.h` framed snapshots, `Node.cpp` pending/job/checkpoint state | NODE, STORAGE, HARNESS |
| 14 Atomicity/wear | Async replacement, uncertain commit reload, retained fences, no-op write avoidance | STORAGE, NODE, HARNESS 304/no-write; physical wear/cuts pending |
| 15 Enrollment/sync | `Node.cpp` enroll/session/sync and bounded control chunks | HARNESS, CAP |
| 16 Protocol bounds | `JsonGuard.h`, `Policy.h`, schemas/fixtures, bounded response negotiation | HTTP, NODE, CAP |
| 17 State machine | `Node.cpp`, `Node.h`, app boot confirmation gate | NODE, HARNESS |
| 18 Scheduling/fairness | `Core.h` cadence/coalescing/admission; Node single transaction/job | NODE, HARNESS; measured target lag pending |
| 19 Bounded maintenance | Both target `maintenance.mjs` adapters and scoped auth/continuations | CAP-M, SCHED, HARNESS; production deployment verification pending |
| 20 Scheduler | Scheduler durable bounded tick/receipt/continuation integration | SCHED, HARNESS |
| 21 Capabilities reconciliation | Capabilities bounded reconcile/pump phases, gate, retention | CAP, CAP-M, HARNESS |
| 22 Claims/execution | Durable Node claim/prepare/start/result ledger and service attempt fences | HARNESS, CAP |
| 23 Wake/effect separation | Cadence coalescing; target receipts/serialization; durable effect ledger | NODE, CAP-M, SCHED, HARNESS |
| 24 Recovery/cancellation | Node reboot reconciliation/cancel; service acceptance and late evidence | HARNESS, CAP |
| 25 Observations/source | Node conditional GET, bounded projection, accepted checkpoints and gap semantics | HARNESS 304 and polls; CAP observation validation |
| 26 Optional transformation | No transform offer, interpreter or upload API | not_applicable for this release |
| 27 Configuration | `Policy.h`, closed schemas, generated policy, credential references | NODE; bundle fixture validation |
| 28 Resource/timing budgets | `Core.h`, `Policy.h`, transport/body/journal bounds, low-water policy | NODE, HTTP, CAP/SCHED bounded budgets; physical measurements pending |
| 29 Initial install | Package, seed and serial scripts; exact boot grants/provider closure | ELF, INSTALL, PACKAGE; clean physical install pending |
| 30 Update/provisioning | Paired serial update, selector-last, independent app-data, boot health gate | INSTALL, NODE; physical interruption/rollback pending; network OTA absent |
| 31 Visibility/control | Node bounded status/diagnostics and authenticated desired controls | NODE, HARNESS, CAP; physical counters/lag capture pending |
| 32 Security/degradation | Scoped policy/tokens, native TLS, retained and corrupt-storage barriers | HTTP, STORAGE, NODE, CAP-M, SCHED; hardware at-rest protection not enabled |
| 33 Work packages | Runtime, provider/app, service adapters, complete package scripts and docs | Evidence above; final source/upload identity recorded in release handoff |
| 34 Profile/transport scenarios | S3-01–12 below | Host evidence and explicit physical/live boundaries |
| 35 Durability scenarios | S3-13–24 below | Fault injection and real service domain modules |
| 36 Integration scenarios | S3-25–36 below | Production-code harness and service tests |
| 37 Lifecycle/load scenarios | S3-37–48 below | Software tests; sustained physical load pending |
| 38 Completion | Four independent statuses in release handoff | No physical test used as substitute for build/artifact delivery |
| 39 Binary handoff | ZIP, all selected-profile images/ELFs, manifest, scripts, fixtures, evidence | PACKAGE plus durable remote download/hash verification required |
| 40 Source basis | Spec pin, runtime/System Apps/SDK identities and service revisions | `flash-manifest.json` and release source/evidence record |

## S3-01–S3-48: execution record

`passed` applies only to the execution layer named. When a scenario has both software and external evidence, the latter is separately `pending_external_environment`. There are no asserted physical passes. An absent optional feature is `not_applicable`, not a claimed implementation.

| ID | Software status and actual evidence | External portion |
| --- | --- | --- |
| S3-01 | passed — target profile build and installer geometry/layout validation (NATIVE, INSTALL) | pending_external_environment — inspect actual module, flash and PSRAM |
| S3-02 | passed — selected cohort has no camera dependency/grant (ELF, package boot manifests) | pending_external_environment — camera-free physical boot |
| S3-03 | passed — headless outbound station/provider design; no listener/AP in selected cohort | pending_external_environment — physical network surface capture |
| S3-04 | passed — memory admission, conservative low-water/overload rejection (MEMORY, NODE, HTTP) | pending_external_environment — missing PSRAM/physical allocation faults |
| S3-05 | passed — provider-bound namespace/admission rejection and ELF imports (STORAGE, ELF) | — |
| S3-06 | passed — lost enrollment response recovers the participant (HARNESS) | pending_external_environment — live credentialed enrollment |
| S3-07 | passed — fresh-session reboot recovery and old-session rejection (HARNESS, CAP) | — |
| S3-08 | passed — participant/owner isolation; target-scoped maintenance authorization (CAP, CAP-M, SCHED) | pending_external_environment — deployed target credentials |
| S3-09 | passed — native TLS validation configuration and bounded failure paths (HTTP) | pending_external_environment — expired and wrong-host certificates on actual transport |
| S3-10 | passed — time suffix/bootstrap/cached UTC tests and monotonic cadence logic (HTTP, NODE) | pending_external_environment — target time bootstrap failure/jump |
| S3-11 | passed — redirect/header/parser validation in bounded native transport (HTTP) | — |
| S3-12 | passed — response parser limits, malformed/chunked handling and strict JSON (HTTP, NODE) | — |
| S3-13 | passed — corrupt/missing storage and journal block without identity reset (STORAGE, NODE) | — |
| S3-14 | passed — every-chunk replacement faults and old/new snapshot recovery (STORAGE) | pending_external_environment — actual power cuts |
| S3-15 | passed — both COMMIT_UNKNOWN outcomes, stale revision and reload gates (NODE, STORAGE) | — |
| S3-16 | passed — retained stat/read/replace/close fences block later I/O and teardown (STORAGE, NODE) | — |
| S3-17 | passed — quotas, journal bounds, shared capacity and rejection before admission (STORAGE, NODE, CAP) | pending_external_environment — physical volume/endurance stress |
| S3-18 | passed — unchanged 304 heartbeat does not write the journal (HARNESS) | pending_external_environment — measured device write/wear report |
| S3-19 | passed — same operation replay, claim/resource holds and result idempotency (CAP, HARNESS) | — |
| S3-20 | passed — committed claim with lost response recovers grant (HARNESS, CAP) | — |
| S3-21 | passed — uncertain effect retains capacity; lost prepare response reconciled (CAP, HARNESS) | pending_external_environment — actual network loss at external effect boundary |
| S3-22 | passed — lost accepted-result response returns original receipt and clears custody (HARNESS, CAP) | — |
| S3-23 | passed — lease expiry/revocation fences and retained uncertain capacity (NODE, CAP) | pending_external_environment — board disconnect at lease boundary |
| S3-24 | passed — expired attempt keeps late evidence without accepting/overwriting (CAP) | — |
| S3-25 | passed — bounded snapshot cursor, restored epoch admission barrier, recovery (CAP) | pending_external_environment — deployed restore with device reconnect |
| S3-26 | passed — cancellation/result race retains candidate and quiescence responsibility (CAP, HARNESS) | — |
| S3-27 | passed — policy overload rejection and bounded target adapters (NODE, CAP-M, SCHED) | pending_external_environment — measured 19-second target/15-second cadence experiment |
| S3-28 | passed — awaited target quanta with committed continuation and durable progress (SCHED, CAP-M, HARNESS) | pending_external_environment — deployed target progress |
| S3-29 | passed — cadence coalescing and target-owned catch-up (NODE, SCHED) | — |
| S3-30 | passed — target-wide lease/gate and replay conflict tests (SCHED, CAP-M) | pending_external_environment — two physical wake providers |
| S3-31 | passed — bounded transport deadlines, fairness/coalescing and overload policy (HTTP, NODE) | pending_external_environment — physical hung-target lag/fairness measurement |
| S3-32 | passed — bootstrap maintenance and ordinary assignment/dependency sequence coexist (HARNESS); provider dependency cycles rejected (CAP) | — |
| S3-33 | passed — shared inventory and no oversubscription of shared pools (NODE, HARNESS, CAP) | pending_external_environment — actual internal/DMA/PSRAM/stack headroom |
| S3-34 | passed — distributed dependency holds, partial composition and cancellation/recovery fences (CAP, HARNESS) | pending_external_environment — physical node loss in composite |
| S3-35 | passed — shared HTTP/MCP/Bus mutation receipt and fragmented/out-of-order reply replay (CAP) | pending_external_environment — deployed bus interruption |
| S3-36 | passed — real domain modules/schema fixtures and common transport receipt semantics (CAP, HARNESS) | pending_external_environment — deployed HTTP/MCP/Bus mappings and correlated live results |
| S3-37 | passed — 304 treated unchanged, no replacement/no flash write (HARNESS) | — |
| S3-38 | passed — durable accepted-result replay/checkpoint path and observation validation (HARNESS, CAP) | pending_external_environment — live source change/report loss |
| S3-39 | passed — explicit observation gaps, source sequence/backpressure and bounded local state (CAP, NODE) | pending_external_environment — long offline source/buffer experiment |
| S3-40 | not_applicable — optional transform is absent from the catalog and executable cohort | — |
| S3-41 | passed — bounded jitter/backoff and durable reboot/session recovery logic (NODE, HARNESS) | pending_external_environment — long real Wi-Fi outage and recovery |
| S3-42 | passed — two-phase rotation receipt recovery, old-work denial and revocation confirmation (CAP) | pending_external_environment — device rotation interrupted on real transport |
| S3-43 | passed — update interruption prefixes, pair integrity, selectors and boot health (INSTALL, NODE) | pending_external_environment — physical interrupted serial update/rollback; network OTA not_applicable |
| S3-44 | passed — exact ranges, immutable app-data, layout/pair validation and selector-last update (INSTALL, PACKAGE) | pending_external_environment — serial flash/readback on actual board |
| S3-45 | pending_external_environment — sustained 72-hour mixed workload not run | Physical soak, retained heap/handles, queues, cadence and memory |
| S3-46 | passed — software replacement/restart fault injection and four harness reboots (STORAGE, HARNESS) | pending_external_environment — brownout, forced reset and physical custody-boundary cuts |
| S3-47 | passed — overload/low-water admission protects control and shared reservations (NODE, CAP) | pending_external_environment — physical saturation and control-lag measurement |
| S3-48 | passed — target compilation, software integration harness, package verification and supplied install/provision path | pending_external_environment — actual clean install and credentialed real-service assignments; durable publication verified separately in handoff |

## Bounded follow-up qualification

1. Record exact board/module revision, chip, flash/PSRAM sizes and modes, power supply, serial identity and image hashes. Run installer preflight, clean install and boot-health checks with real scoped enrollment and target credentials.
2. Exercise live enrollment/restart, inventory, both maintenance endpoints, a health/source assignment, cancellation, dependency composition and acceptance recovery. Correlate HTTP/MCP/Bus receipts without publishing secrets.
3. Run 72 hours of mixed work and controlled Wi-Fi loss. Capture requested/started/completed/timed-out/unknown counts, actual maintenance intervals/lag, contact gaps, backlog progress, reconnect delay, reset causes, minimum/largest internal/DMA/PSRAM blocks, stack headroom, handles, queue depth and write counts.
4. Cut power around state replacement, dispatch, result acceptance and receipt custody; interrupt serial update before and after selector change. Verify old/new complete snapshots, retained unknown effects, pair rollback and unchanged independent data. Measure write amplification/endurance rather than infer it from host no-op tests.
5. Exercise missing PSRAM, allocation pressure, TLS date/hostname/time failures, credential rotation interruption and a hanging target. Refine low-water and timing bounds from the measurements. Record failed checks with their evidence; do not convert pending checks to passed because time elapsed.

These follow-ups do not postpone delivering compiled software and a verified install bundle.

## v1.2 captive setup amendment

Explicit user request authorizes a temporary on-device AP, wildcard DNS and captive HTTP portal, replacing the original unprovisioned terminal block. The ELF owns all DNS/HTTP parsing and setup UI. The native optional UDP suffix retains explicit provider generations, bounded nonblocking calls and retained-close fencing. Runtime target flags enable AP transport only in this infrastructure profile.

`test/infra_node/portal.cpp` exercises first setup, HTTP redirects/framing/CSRF, pin validation, durable saves, failed network correction, reboot and uncertain commits. `test/native_tcp_listener_test.cpp` adds UDP size/ownership/cleanup/loss-of-owner coverage. Existing service protocol and recovery tests remain required. Mobile/browser and physical RF behavior remain unqualified until flashed hardware is exercised.

## v1.2.3 setup correction and evidence correction

The owner physically confirmed v1.2.2 AP discovery, phone association and captive
page opening on 2026-10-30. Save was rejected with `Use the setup page.` before
any station join. `POST /setup HTTP/1.1` has 20 characters; the parser incorrectly
required 19. Request-line matching now derives the exact length from each literal
and admits HTTP/1.0 and HTTP/1.1 without accepting truncated/prefix variants.

The earlier portal PASS output was invalid: the vendor SHA configuration leaked
`NDEBUG` through Digest.h and compiled out assertions in tests including it.
Digest.h now preserves the caller's NDEBUG setting, and every infrastructure C++
suite rejects disabled assertions at compile time. With assertions restored,
the old parser fails its existing CSRF test; the corrected parser passes the
full setup/save/retry/reboot/uncertain-commit suite and new exact-line checks.
Earlier affected assertions must not be cited as evidence independently of this
rerun. Core USB/radio tests that did not include Digest.h were unaffected.

The page explains that Android's embedded sign-in window may not provide a file
picker and directs the owner to stay connected and open http://192.168.4.1/ in
Chrome. The file input remains available; paste remains supported and is exposed
in Android WebViews. The local-network explanation cannot be mistaken for a
router internet check; failed saves say settings were not saved and retain input.
Page-script tests execute the delivered JavaScript with DOM/network mocks; they
do not certify Android's native picker. Android's WebChromeClient default cancels
file requests, and the AOSP captive-portal WebChromeClient does not override it:
https://developer.android.com/reference/android/webkit/WebChromeClient#onShowFileChooser(android.webkit.WebView,android.webkit.ValueCallback%3Candroid.net.Uri[]%3E,android.webkit.WebChromeClient.FileChooserParams)
https://android.googlesource.com/platform/packages/modules/CaptivePortalLogin/+/refs/heads/main/src/com/android/captiveportallogin/CaptivePortalLoginActivity.java

The four immutable provider definitions advance to revision 3 with the rebuilt
infra-node hash. First setup accepts the exact published v1.2/v1.2.1/v1.2.2
pairing cohort and substitutes this image's implementation pins; tampered/mixed
older cohorts remain rejected. Contract pins, authority credentials, endpoints
and identity are preserved. Existing paired-device identity replacement remains
prohibited. No new pairing file is required for the owner's existing download.
Physical save/join/service reconciliation remain pending.

## v1.3.0 relay and Scheduler budget verification

The relay host tests execute the production Node state machine under ASan/UBSan.
They cover durable intent before POST, reboot before and after local success,
ambiguous remote success with the same idempotency key/body, two-target progress,
monotonic subscription updates and rejected origin/credential escapes, bounded
five-attempt retry, dead-letter confirmation before ACK, and health during backoff.
An independent Python HMAC calculation checks the actual signed bytes.
LeakSanitizer is disabled because the host ptrace environment does not support it.
The full service harness passes 173 calls, 28 maintenance wakes and four reboots.

The Scheduler remote-database regression advances simulated D1 round trips by
180 ms: a one-second budget cannot commit the due work, while the eight-second
policy commits one responsibility and replays without duplication. The replay
timestamp is fixed outside the lazy-read callback and admission latency is
accounted separately. The merged Scheduler tests and built Worker test pass.
Channel Bus relay tests include a built Worker/D1 allow/deny test for its scoped
credential. Capabilities decision outbox tests confirm the narrow business-key
routing field without leaking the answer or other source fields.

The target application and native runtime compile for the pinned ESP32-S3 SDK;
the native main-task stack is 24 KiB and the admission floor stays 4096 bytes.
These are software results. Physical stack headroom, webhook latency, Wi-Fi
outage, brownout recovery and live Grok receiver delivery remain unqualified.
Grok's routine endpoint and private credentials have not been supplied.
