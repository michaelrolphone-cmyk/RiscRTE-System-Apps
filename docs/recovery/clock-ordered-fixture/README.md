# Clock CI ordered-provider fixture correction

Base: local System ec4c3038b6a6368b2fe990247b2c32752c670cc6. Production
Apps/, lib/, target build recipes, and workflow selection are unchanged.

## Original failure

`python3 scripts/test_paper_clock.py` failed immediate scene 1 at the required
`launches == 1` springboard assertion. The old fixture changed sequence-zero
snapshots but its `next()` always returned empty. The ordered consumer correctly
refuses inconsistent snapshot-only changes. This is retained in
`original-failure.log`, and the unselected legacy provider remains executable
as the negative control.

The selected test-only provider now supplies a bounded sequenced event queue,
matching snapshots, valid distinct contact IDs, and elapsed-time scripts.
Only scheduled changes are hardware reports; idle capture polls invent no
reports. Blocking display waits advance wall time without advancing the user
script, so a fake initial 1.5 second display wait does not skip the user's
future gestures. All original launch, no-launch, frame, error, and cleanup
assertions remain. Handoff additionally requires no writable or pending frame.

## Focused results

- Clock: original 84 normal/ASan+UBSan cases plus 24 added cases, all passed.
- Added cases: 2 ms swipe, valid same-report MOVE plus second DOWN cancellation,
  2 ms non-launching tap with both delivered edges asserted, fresh swipe after
  simultaneous-contact cancellation.
- All 14 original display failure cases passed.
- Paper Quick: 124 Clock/launcher cases and both 59-check Home reducers passed.
- Flipped paper Quick: 124 cases passed.
- Launch guard: all 36 cases passed unchanged.
- Power status: both normal and ASan+UBSan passed with `ASAN_OPTIONS=detect_leaks=0`.
  Unmodified local invocation encountered this executor's LeakSanitizer/ptrace
  restriction; its original output is retained. No sanitizer is disabled in CI.

Run `python3 scripts/test_paper_clock_fixture_controls.py` for the source-bound
positive and three negative controls: legacy empty event stream fails the
original launch assertion; corrected stream passes; dropping its edges fails;
a temporary app-source mutation raising the horizontal swipe threshold fails.
The mutation does not edit production files. Digests are in `controls.json`.

## Separate finding and limits

A later unchanged two-contact report can advance the authoritative snapshot
past a queued simultaneous MOVE/DOWN report. The frozen reducer's equality-only
snapshot timestamp test may then let the MOVE launch before second-DOWN
cancellation. That delayed-snapshot case is NOT counted among the passing cases
above. Its actual selected-provider reproducer is a separate investigation.
This fixture-only correction does not claim that production issue is fixed.
Hardware behavior and target recipes were not changed or requalified here.
