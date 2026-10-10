# Separate production finding: later unchanged GT911 report

This is a failing production regression reproducer, separate from the old
snapshot-only CI fixture correction. No production source or target recipe was
edited. The suspect scenario is recorded as FAIL in both normal and ASan+UBSan
runs, not counted as qualified.

## Exact selected provider

- Product: X4 .57, GT911 version 0.1.9
- Driver SHA256: da287cbd675b6a59131b97278a48c6cf1734b90e30a9e2674e72b0063c9a86d1
- System: fixture correction 3fd3976b346d02115577ba673e71bd2fd85e8590,
  production inherited unchanged from local ec4c3038b6a6368b2fe990247b2c32752c670cc6
- Actual Clock app, portable adapter, ordered reducer, and GT911 driver compiled
  together. Only the physical I2C/GPIO/clock environment is the selected product's
  host fixture.
- The old Clock host profile needs its local legacy battery header; that profile
  and the GT911 source share one staged set of raw-touch/provider headers. Every
  staged header's digest and all primary source digests are in evidence.json.

## Dispatch trace

1. A ready report produces DOWN id1 at (240,360), seq1, timestamp 1050.
2. A second ready report produces MOVE id1 to (285,360), seq2/t1075, followed by
   DOWN id2 at (320,440), seq3/t1075. Both fingers are present.
3. A valid unchanged ready report contains those same two fingers. It emits no
   edge, preserves seq3, and advances the authoritative snapshot to t1076.
4. Clock's ordered dispatch reads MOVE seq2/t1075 at runtime ms1077. Its snapshot
   is seq3/t1076 with two valid distinct contacts.
5. PortableTouch's MOVE guard requires snapshot timestamp == event timestamp,
   so this MOVE is delivered rather than cancelled. Clock requests springboard
   at ms1078 before the second DOWN can be dispatched. The unchanged original
   no-launch assertion aborts.

The fixture uses a second independent GT911 subscriber to trace every produced
edge without draining or editing Clock's queue. It also records each Clock
next() delivery and authoritative snapshot at that exact dispatch boundary.
See later-unchanged-report-0.log and later-unchanged-report-1.log.

## Positive controls

Normal and ASan+UBSan each pass:

- Identical simultaneous-contact sequence without the later unchanged report:
  cancellation occurs and no launch happens.
- Full Clock 2 ms swipe: exactly one intended springboard launch.
- Full Clock 2 ms tap: DOWN and UP delivered, no unintended Clock launch.
- Actual-provider same-tick recontacts: both completed taps remain eligible.
- Actual-provider full-queue same-tick burst: all 16 taps remain eligible.

Totals: 10 passing controls; 2 reproduced failing regression cases. No physical
hardware assertion is made.

## Reproduce

From System:

    python3 scripts/test_paper_clock_gt911_reports.py \
      --product /workspace/shared/x4-wifi-ui-composition-057 \
      --sdk /workspace/shared/x4-057-ci-sdk-proof/target-final/sdk \
      --output-dir /tmp/clock-gt911-report-regression

The default investigation command records the known failure and preserves its
trace. Add --require-fixed to demand all cases pass and fail the command while
the regression remains. This runner is not added to ordinary CI as a green
qualification gate.
