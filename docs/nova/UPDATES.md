# Firmware Update and App Store

The Watch build selects `--nova-ui` and portable version 1.1.1 for each app.
It shares Settings typography, cyan/teal palette, rounded buttons, 54px list
rows, 44px footer controls, header Back and adapter gestures. The published
Reader implementations and their manifests remain separate and unchanged.

The shared controller retains explicit two-step installation confirmation.
Selecting a row or touching the body does not install anything. Nested Back
leaves confirmation; root Back cancels and releases update/Wi-Fi resources
before returning. Unknown activation stays restart-only and never reports
success. Cancel is handled before the next bank step and redraws the actual
service state. Alarm polling, cleanup retention and sleep barriers remain.

`test_portable_update.py` runs both applications, both touch orientations,
legacy and Nova views, normal and ASan/UBSan builds: 38 UI scenarios in each
combination. Cases include cancellation at verification, uncertain activation,
radio/HTTP cleanup failure, alarm interruption, retained sleep, two-row page
geometry, 44px footer selection and nested header Back. The target builder
checks imports/exports and validates Xtensa ELFs. No hardware/network action
is part of these tests.
