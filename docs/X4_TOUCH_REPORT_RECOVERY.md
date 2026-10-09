# X4 touch-report recovery

The X4 0.1.29 native touch consumer called terminal invocation retention for any failed poll or negative event result. The raw touch contract allows poll failures with partial progress and defines next() == -1 as a recoverable queue gap or stale subscription. This repair drains the bounded event stream, checks the current snapshot, cancels derived gesture state, and requires neutral input before accepting a fresh tap. Provider faults (next() < -1) and failed snapshots still retain the native invocation.

Native terminal errors now emit TOUCH fault operation=next|snapshot result=... action=retain before invoking the existing retention operation. No further touch operation follows a terminal error. The provider ABI and its hardware behavior do not change. The header preserves the shipped Home and optional scroll fields so this correction can be consumed by the completed X4 native profile, rather than removing those features during publication.

Local validation: the source-matched 0.1.29 GT911/consumer integration passed 24 original/repaired runs with modeled GPIO/I2C/clock. The added repository test passes ten scenarios in native and legacy modes, normally and under ASan/UBSan: 40 runs. It checks poll failure, queue gap, combined loss, exhausted queue budget, multiple contacts, bad coordinates, true and unknown provider faults, failed snapshots, and diagnostics before retention. The dedicated CI also compiles the production native consumer with Xtensa ESP32-S3 GCC 8.4.0. Its object is a compile-test artifact, not a firmware image.

This shared-source PR does not claim a full X4 0.1.29 product rebuild, a Bluetooth over-the-air test, multi-contact support in the X4 GT911 provider, or a battery-only startup repair. App build pins must select this corrected header when the X4 native app cohort is rebuilt. The ordinary repository app-build workflows run on the PR independently of the focused native-consumer workflow.

Reproduction: python scripts/test_touch_report_recovery.py
