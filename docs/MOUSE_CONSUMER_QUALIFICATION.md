# Bounded copied mouse consumer qualification

This successor preserves PR151 at c75f0b617e55d45f84409efb93f442385035477c,
including its original source, historical incomplete checkpoint and recovery
bundle. It adds the absent native integration runner and exact-head CI without
transplanting later System Apps cohort scripts from PR155. Qualification is
pending until an exact-head result is recorded in the draft PR.

Inputs remain Runtime 6a3b3d46f5b2857eacc6e9863799dec1f466b036 and Reader
59177418ea746c7605b7b5a5aec501e70bdae80f (provider source
c334919a05d393b7b3a536fceb117cfbb573a4ef). Reader PR468 and all ABI/source
bytes are unchanged. The runner compiles the real mouse driver and generic
Runtime graph, using a test-only HID provider and a clock table registered through the
existing port API. Platform clock is not a substitute driver ELF: the unchanged
Runtime refuses that dependency without trusted port registration. An explicit
negative control preserves this refusal before any driver starts. No Runtime
admission source or trusted product table is edited.

Separate processes exercise normal, still-connected close, failed close and
missing-platform refusal with eager/demand activation. Normal includes a fresh
semantic session and generation-qualified device token after re-enumeration, copied-once motion, focus
loss and fresh baseline, queue overflow/GAP/snapshot recovery, and disconnect
release. Tests cover maximum-capacity CONNECTED rejection, malformed
provider-free invocation tags and rejected retention fences without retries.
Retention assertions establish Runtime retained state, exact raw test-token
custody, no dependency stop and app-fini suppression where applicable; they do
not instrument or prove target executable mapping or physical quiescence.

This is not lower-host/physical USB qualification. The host shared objects are
not Xtensa product binaries. No producer cadence, target Runtime admission,
product input routing, cursor/focus policy, X4 VBUS/role/electrical behavior,
firmware, release, merge, or product pin is selected or qualified. Raw HID close
never establishes lower-host quiescence. Retained graphs are deliberately not
destructed or retried; process exit is test disposal, not product recovery.

Run normal and sanitizer qualification with:

```sh
python3 scripts/test_portable_mouse_consumer.py --runtime /path/to/Runtime --reader /path/to/Reader --output /path/to/evidence-normal
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 python3 scripts/test_portable_mouse_consumer.py --runtime /path/to/Runtime --reader /path/to/Reader --output /path/to/evidence-sanitized --sanitize
```

Both upstream checkouts must be clean at the exact revisions above. The
sanitizer mode enables AddressSanitizer and UndefinedBehaviorSanitizer; this
lane makes no LeakSanitizer claim.

The normal lane compiles at `-O1`. The sanitizer lane keeps the consumer and
provider C at `-O1` and compiles the Runtime C++ host harness at `-O0`, with
`-Wall -Wextra -Werror` and sanitizer instrumentation retained. The unchanged
pinned ArduinoJson empty iterator produces a GCC `maybe-uninitialized`
diagnostic at `-O1` with sanitizers. This profile adjustment does not patch
upstream source, disable a warning, or qualify optimized Runtime sanitizer
behavior.

Historical failures remain available and are not overwritten by later runs:

- [Run 39670339438](https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/actions/runs/39670339438),
  head `4799a04d17399e24ea8d6a219e143b55b13782e7`: native integration did not
  compile because the fixture used absent `Runtime::lastError`; sanitizer step
  skipped. Artifact `12378579185`, ZIP SHA-256
  `a074d5af99e2ca2bf56d74cc6e758a23f9c99b775e0b66dee9e0d940b00f55ea`.
- [Run 39674048111](https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/actions/runs/39674048111),
  head `28b501b4d4557bc7f1fc30d87cd50ee60e82a809`: normal qualification passed;
  sanitizer consumer cases passed but Runtime compilation stopped at the
  ArduinoJson diagnostic before integration execution. Artifact `12380433812`,
  ZIP SHA-256 `ae6929538b779b91d3f295731999845d34afabca4cd327c7f83ea5c54296ea72`.
