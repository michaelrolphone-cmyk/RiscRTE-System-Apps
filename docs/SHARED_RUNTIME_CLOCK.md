# Shared Runtime clock adoption

`lib/PortableApps/include/RiscRuntimeClockV1.h` is an exact vendored SDK file:
Runtime commit `37fca3e2244bf71ac64bd8cd7e62eb94f4c09a9d`, path
`sdk/app/RiscRuntimeClockV1.h`, SHA-256
`76584fdab224b184918d9f9cf675d5c4443adef7161e8518d456bc25277dc30f`.
The canonical implementation remains Runtime-owned. Vendoring makes historical
System builders that stage the PortableApps headers inherit the same helper
without changing their pinned Runtime profiles. No native ABI/provider changes.

Adopted consumers (not a universal migration claim):
- PortableApps adapter `millis_now`, retaining its failed/sparse-phase policy;
- installed catalog maintenance backoff and periodic rescan timers;
- Contexts cache/checkpoint timestamps and Broadcast policy-read pacing;
- timestamp-only `PortableStageLog` diagnostics.

Copied Runtime tables now replace a present monotonic suffix with app-local
custody or sparse-clock wrappers, just as they already replace health. These
wrappers enforce existing retained and file-setup guards, commit output only on
success, and retain on provider failure. Old prefix sizes stay old; a null suffix
stays null. The helper uses health only when the suffix is absent/null, never
when a present callback fails. Zero and wrapping uint32 milliseconds are valid.
Genuine health reporting and authority-only OptionalService callers are unchanged.

Checks:
- `scripts/test_guarded_monotonic.py --sdk <runtime>/sdk/app --output <dir>`
  executes extracted exact production wrappers, copied-table initialization and
  millis reader: normal/sanitized custody and file-setup cases. Not full adapter
  integration or physical proof.
- `scripts/test_shared_runtime_clock.py --runtime <canonical-runtime> --output <dir>`
  verifies exact header equality, actual Contexts/Broadcast/StageLog timing and
  installed-catalog timer callbacks under normal/ASan/UBSan. No target/device claim.
- Existing Broadcast client and installed catalog tests pass. The existing
  Context client aggregate fails its cached radio-policy assertion at line93
  on both untouched base83da63 and this successor; this change does not repair or
  claim qualification of that pre-existing policy-cache mismatch.

The canonical SDK's separate current/old-header C/C++ tests cover compilation
against SDKs without the monotonic suffix. Consumer model checks do not establish
hardware time accuracy, RTC truth, or physical power behavior.
