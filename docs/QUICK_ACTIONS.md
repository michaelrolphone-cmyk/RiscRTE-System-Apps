# NOVA-7 quick controls (opt-in Watch profile)

`PORTABLE_QUICK_ACTIONS` selects one shared controller, RGB565 renderer and
invocation-local modal in PortableApps. The Watch Clock uses the same files.
The final Watch builder links `quick_actions.c`, `quick_render.c` and
`quick_session.c`; legacy standalone profiles do not silently adopt it.

A downward drag beginning in the top 36 pixels opens the panel. It follows the
finger with bounded fixed-tick spring settling. A tap or rejected horizontal
or upward contact replays to the original app. Other app touches are unchanged.
Swipe up or tap the handle to close. Crown Back closes the panel first. No
keyboard is added. The real foreground stack, draft and pixels survive closing.

- Brightness: 10–100%, immediate preview, one verified save per released drag.
  Missing preference defaults to the existing 40%; no startup writes.
- Notification volume: existing namespace-1 `alarm_volume`, default 50%, zero
  mutes sound without changing vibration. Alarms, Countdown and Points cues use
  it. Frequency Generator keeps its own explicit level.
- Silent: saves the prior nonzero level before muting; unmute restores it even
  across a fresh invocation. Failed writes are visibly unconfirmed.
- Torch: transient warm-white light at full brightness. Tap, Back, alarm modal,
  sleep or exit restores the saved brightness. Torch state is never persisted.
- WI-FI SETUP: ordinary navigation to the existing Wi-Fi Settings app. It is
  clearly navigation, not a claimed system-wide radio toggle.
- DND, Airplane and Bluetooth remain visibly unavailable. Their global policy
  and exceptions are not defined by the existing app-owned providers. This
  candidate is not completion of those integrations.

The profile grants namespace 1 and read-only RTC capability selection to each
client, preserving every prior grant. Runtime's policy must support nine exact
grants for the two update apps. No raw hardware permissions or new persistent
credentials are added. Storage calls occur at settled service/display boundaries;
alarms retain priority and cancelled contacts never leak through the modal.

`python scripts/test_quick_actions.py` executes pure core and real-adapter tests
at both touch rotations, under strict warnings and ASan/UBSan. It emits actual
native-render PNGs under `build/quick-actions`. Coverage includes foreground and
nested Settings draft retention, top-edge replay, every slider/tile path,
notification preemption, corrupt/uncertain persistence and cancellation.
`generate_quick_assets.py` regenerates bounded raster masks from retained licensed
Font Awesome, Orbitron and Rajdhani sources. Firmware has no SVG/font parser.
