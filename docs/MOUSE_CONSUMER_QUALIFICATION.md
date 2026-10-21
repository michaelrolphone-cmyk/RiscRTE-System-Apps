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
Runtime graph, using test-only HID/clock dependency witnesses. Separate
processes exercise normal/failed-close cases with eager/demand activation.

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
