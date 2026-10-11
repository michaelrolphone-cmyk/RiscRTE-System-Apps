# USB preparation integration fixture repair

The .078-era preparation harness overrode only the raw touch snapshot, while
retaining paper_clock_test.c's poll/ordered event callbacks. The adapter consumes
ordered touch events, so the intended USB Start tap never occurred. Its early
Home event then launched with no USB lease ever acquired, failing the fixture's
assertion that a USB release had occurred. The same failure reproduces with
Reader MSC 45cf61ac and cleanup candidate 4c2e2dbe; transport.c is not linked by
this test.

The fixture now feeds its existing scenario into both the ordered event queue
and snapshot state using the base fixture's raw_emit/raw_find helpers. All
ownership, settled-frame, per-input SD preparation, exactly-one-release and Home
assertions remain unchanged. Production app, adapter and provider code do not
change, so no component version bump or .078 payload replacement is needed.

All four modes pass normally and under UBSan: successful preparation, cancel,
clean refusal and retained failure followed by retry. On this macOS run the
invocation used Darwin dead stripping and disabled TinyUSB alignment checking;
ASan and Linux-native execution are not implied by those results.
