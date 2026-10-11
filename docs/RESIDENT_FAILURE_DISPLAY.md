# Resident failure display

The selected resident System host paints an explicit diagnostic page after a
foreground has failed and Runtime has completely cleaned it up. It uses the
copied `risc_resident_result_v1.failure` record: application path, failure step
(load, ABI, initialization, allocation or cleanup), detail, status and invocation
identifier. A refusal with no matching record stays on the existing refusal path;
it cannot display an earlier application's failure. No diagnostic callback is
installed in Runtime, and Runtime remains headless.

The page replaces the whole image using a CLEAN presentation when the display
supports it, and waits for the presentation to complete. It stays visible until
a fresh Home/Back/Confirm press or a deliberate tap in Continue. Inherited held
input and taps outside Continue do not dismiss it. The acknowledgement is
neutralized before normal Home returns. Alarm servicing continues while the page
is visible. The existing launch, Home, sleep and power-policy behavior resumes
after acknowledgement; this feature does not request a reboot.

At the first settled host launch boundary, the host also reads the optional
`last_failure` API. Only a correctly sized PRIOR_RESET record with a nonzero
native reason is shown. The current ESP port supplies panic/watchdog/brownout
reset evidence from `esp_reset_reason()`, with no application identity or stack.
Those missing fields are explicitly displayed as not recorded. Normal resets,
missing evidence and hosts without the optional callback add no page.

## Safety and limits

- RETAINED is checked before restoration, focus, touch, rendering or any other
  provider work. A retained child does not trigger this display.
- Display acquire/surface/submit/status/timeout failure uses the existing custody
  fence. No retry, alternate provider, resource release or further I/O follows.
- Native panic, watchdog, brownout, host initialization failure, and failure before
  a healthy System host owns the display cannot be painted before reset by this
  change. E-ink can still retain the previous image in those cases.
- The available ABI contains no stack trace or recorded sequence of calls. The
  page shows the actual failing lifecycle step and invocation, without inventing
  a stack, application identity or a persistence guarantee for session failures.
- The prior-reset check is once per host invocation. The read-only Runtime API
  has no acknowledge/consume operation, so a fresh host after a legacy handoff
  can show the same reset record again until a new native boot or Runtime failure
  replaces it. No persistent user settings are written to suppress evidence.
- This is opt-in resident source, not a new Runtime, firmware image or release.
  Hardware presentation, native reset retention and early-boot recovery remain
  unqualified. The accepted .43 image and the separate renderer worktree are not
  modified by this isolated implementation.

## Reproducible checks

`scripts/test_resident_failure.py` compiles the production Runtime/Graph and real
System host as separate ELF modules, with synthetic admitted display/input/clock/
alarm providers. The 19 cases run normally and with ASan/UBSan:

```sh
python3 scripts/test_resident_failure.py \
  --runtime /workspace/shared/runtime-resident-gpio-0190 \
  --display-sdk /workspace/shared/x4-resident-policy-qualified-target/default/idle-sdk/include
```

The cases cover healthy startup, load/ABI/init failure, repeated failures, a
prior reset, absent reset evidence, maximum-width text and unbroken tokens,
inherited navigation, inherited touch plus
an out-of-button tap, retained child, and acquire/surface/submit/status/timeout/
navigation/touch failure. Providers assert against every call after the real
Runtime retention bit is set. Clean failure cases assert child unload before
return, settled full CLEAN frames, valid image margins, fresh acknowledgement,
and no leaked Home/sleep/launch action. Ordinary text wraps at word boundaries;
long tokens use character fallback. Pathological wide fields are limited to six
lines with a visible ellipsis, and raster checks reserve the footer region. The
runner writes PBM/PNG renders and a
source-hash receipt to `build/resident-failure/`.

The existing resident/legacy suite and the actual System controller/host suite
remain the regression checks for ordinary drawer, navigation, policy and app
lifecycle behavior.
