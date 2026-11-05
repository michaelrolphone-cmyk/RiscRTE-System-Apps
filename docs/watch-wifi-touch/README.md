# Watch Wi-Fi footer responsiveness

The native-height Watch profile renders the Wi-Fi Prev, Select and Next controls
with `WatchViewLayout`: width-scaled coordinates and extra vertical footer space.
The controller used the older centered 240-pixel transform for those same controls.
On a 410×502 display, visible footer taps therefore did not select or activate
rows. Row hit testing and Back already used their appropriate native transforms.

The controller now applies `watch_view_touch` to footer input when both NOVA and
native-height are selected. Other profiles retain their existing coordinates.
The transform rejects the additional vertical gutter and preserves keyboard
geometry.

## Source and reproduction

The preserved Waveshare Wi-Fi ELF is 169,460 bytes, SHA-256
`7bb9d98ace7cb783e6f85368c0dfd29360b96a635766c6f3f637b6901ae0d6c8`.
Its replay receipt binds the app to System source
`96742a20af660808cae6cd3a36eadd12a83a460f`. The controller,
`wifi_nova.inc` and `WatchViewLayout.h` match the corresponding pre-fix files
used by this regression. The production recipe enables `PORTABLE_NOVA_UI` and
`PORTABLE_WATCH_NATIVE_HEIGHT`.

The unchanged controller records one scan start, one cancellation and one Back
return at 240×240. At 410×502 it records zero scan starts and cancellations,
although Back still returns. Applying the shared inverse transform makes both
cases pass. This establishes a concrete ignored-touch fault; it does not prove
that every physical freeze report has the same cause.

## Regression

Run `python3 scripts/test_wifi_watch_touch.py --output build/watch-touch/normal`
and repeat with `--sanitize` and a different output directory. Sixteen cases
exercise the production controller, renderer and input adapter:

- Legacy 240×240 and native 240×240, 410×502 and 480×480 profiles
- Repeated Next and Prev, selection, scan start and cancellation
- Footer gaps and inert message/gutter input
- Back navigation and release of all synthetic provider grants

Radio, storage, display and touch devices are synthetic providers. Hardware
validation remains untested. The full existing Wi-Fi workflow suite remains a
separate regression gate. Product integration must rebuild the Wi-Fi package;
a source merge does not alter previously released firmware bytes.
