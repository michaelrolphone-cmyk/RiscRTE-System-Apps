# Historical WIP: shared Watch launch brightness checkpoint

Historical checkpoint, superseded by [current qualification](README.md).

Status at checkpoint: implementation checkpoint only, paused behind the requested SDR native-resolution and unobscured-plot work. This is not a released or hardware-qualified repair. No product, driver, grant, manifest or native-UI change is included.

## Exact scope

Base System source: fe238022814c89e3c7cafb3df60a3997e34caf7b, whose repaired app adapter is b8086d45ffa8259f982acac5974e7d6e4a6aa88b.

Observed products: Waveshare 0.1.45 (49aecbc116172b89481c6880d74a350643b798ef) and T-Watch 1.1.36 (9974c6ac0c6edd3a9860f18e22bcea54d10d957a), Runtime 4107eaeedd8c801eeaf7deb1c7bc4a943cd27342. Both intentionally reopen their display providers dark. All 36 repaired replay recipes select the shared adapter and Quick Actions; the adapter lacks the first-frame restoration already present in separately preserved Springboard f31f89636304d202c49884e2e9b2e0b2ec5fb891.

## Preserved repair

The ordinary RGB565 adapter restores its current Quick brightness after the first successfully completed presentation, in synchronous and asynchronous paths, once per invocation. Accepted previews and torch remain current. It performs no preference write. Paper light policy and builds without Quick Actions remain unchanged. Failed presentation/brightness and retained callbacks stop at the existing custody fence. The complete existing optional-storage crash repair is preserved.

## Checks completed before pause

- 124 fixed-source cases: 15 RGB cases at 240x240 and 410x502, sync/async, normal and ASan/UBSan, plus four MONO1 no-restoration controls.
- Two no-Quick controls, normal and ASan/UBSan, preserve the existing grants, UI and brightness behavior. These were compiled separately as the final bounded source-safety check.
- Eight unchanged-source cases reproduced a completed RGB frame remaining at zero brightness. This baseline receipt predates the fixture's tagged-alarm compatibility update; the production baseline adapter was unchanged.
- Python syntax and git diff whitespace checks passed.

The receipts record source/test hashes and commands. Leak detection is disabled for this execution environment; address/undefined-behavior checks remain enabled. Host doubles do not qualify physical display output.

## Pending, not passed

The consolidated runner after its no-Quick extension and tagged-alarm baseline update has not been rerun end-to-end. The existing 192-case custody suite has not been rerun on this checkpoint. CI additions are unqualified. Replay source pins, expected ELF hashes, full-cohort target builds, admission, product releases, catalog and hardware checks have not been performed. The replay still intentionally points to its previously released source and bytes.

## Springboard remains unresolved

A separate read-only audit verified the exact official Springboard identities and both provider lifecycle contracts. Existing f31f896 first-frame tests passed all 32 sanitizer cases with stage profiling; exact vendor-facade/panel checks also passed. Its synchronous and asynchronous paths already restore brightness after COMPLETE. No autonomous late-zero write or separate launch race was demonstrated, so no Springboard or driver code is changed here.

For a black Springboard recurrence, establish whether first COMPLETE and a nonzero brightness command occurred. Waveshare already logs PANEL_BRIGHTNESS value=102 for the default 40 percent. Missing completion/unblank indicates an earlier path; successful nonzero command requires inspecting physical output or framebuffer content. No hardware recovery is claimed.
