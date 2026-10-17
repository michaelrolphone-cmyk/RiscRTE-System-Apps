# Published-main union

This follow-on checkpoint preserves both published parents:

- Independent native Quick: `f3bb53c9d8ed458d295aec18fef5d7733df581b1`
- Published main: `fd6bc6065f6e409ed2d2ea1858bd56ab6d9e2fb3`

The merge was clean. Published main adds its existing native Settings implementation,
recipe and tests. Its only shared-adapter edit is the compile guard around the
`settings_atomic_owned` declaration. The native Quick startup, raw-coordinate input,
snapshot admission and native-width work accounting remain intact. There is no
stopped unpublished idle/lifecycle correction, app-selector change or release
publication in this union.

The parent directory's host receipts describe the original fd9-based Quick
checkpoint and remain preserved. Its Quick push and PR CI passed; the broader
Native Settings workflow failed because its recipe expected
`test_native_settings_features.py`, absent at that independent head. That outcome
is historical workflow/source mismatch evidence, not erased by this union.

These union receipts bind the requalified source closure. The newly available,
already-published Settings feature suite runs unchanged. Its existing exclusions
for nested native-idle cases 5 and 6 remain explicit; passing these host tests does
not accept the stopped automatic-idle correction or establish device behavior.

Remaining full-task work still includes shared alarm chrome/paper geometry,
combined app selection, the separately stopped idle correction, target release
integration and hardware qualification. Draft PR124 remains a source checkpoint;
no main merge is performed here.

Existing native-settings-1.4.0-dev1 limitations remain. These published-main tests do not supersede release acceptance.

Local results: 12 adapter configurations, 2 compile rejection guards, 8 Quick selector tests, 54 existing Settings feature executions, 14 existing builder/publication unit tests, and 238 selector-off regression processes passed. The adapter receipt validates 15 explicit source hashes and all 71 non-system compiler dependencies.
