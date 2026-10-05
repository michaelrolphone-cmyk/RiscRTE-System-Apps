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
- Wi-Fi: persistent enable/disable policy shared by Wi-Fi Settings and update
  clients. Off cancels/drains current radio work and prevents scans/joins until
  enabled. On permits connection using the existing Wi-Fi Settings flow; it
  never creates credentials or claims to be connected without an existing link.
- Bluetooth: actual controller On/Off, queried through a size-gated driver
  extension. This does not implicitly advertise, pair, or run a host protocol.
- Airplane: turns Wi-Fi and Bluetooth off and saves their prior preferences;
  turning it off restores them. Individually enabling a radio clears Airplane.
- DND uses the selected "Silence everything" behavior. Its independent namespace-1
  `alert_dnd` record is one byte, 0 or 1; missing means off without an implicit
  write. Malformed or unreadable records disable the tile. Confirmed writes
  refresh alarm.service; that service owns suppression of alert occurrences.
  DND never changes Silent, volume, brightness or radio preferences.
  The initial non-radio test profile retains the honest Wi-Fi Settings shortcut;
  the complete current deployment selects PORTABLE_QUICK_RADIOS.

The profile grants namespace 1 and read-only RTC capability selection to each
client, preserving every prior grant. Runtime's policy must support ten exact
grants and nine capability types for the two update apps. No raw hardware permissions or new persistent
credentials are added. Storage calls occur at settled service/display boundaries;
alarms retain priority and cancelled contacts never leak through the modal.

`python scripts/test_quick_actions.py` executes pure core and real-adapter tests
at both touch rotations, under strict warnings and ASan/UBSan. It emits actual
native-render PNGs under `build/quick-actions`. Coverage includes foreground and
nested Settings draft retention, top-edge replay, every slider/tile path,
notification preemption, corrupt/uncertain persistence and cancellation.
`generate_quick_assets.py` regenerates bounded raster masks from retained licensed
Font Awesome, Orbitron and Rajdhani sources. Firmware has no SVG/font parser.

Radio policy is one checked four-byte namespace-1 record, so Airplane/restoration
cannot persist a torn pair of preferences. Radio changes are applied and verified
before save; an unconfirmed save tries to restore the previous hardware state.
An unreadable/corrupt saved mode quiesces both radios without overwriting the
record; controls remain unavailable until the preference can be read safely.
Unconfirmed native cleanup retains the invocation. Healthy persistent Bluetooth
survives app navigation; explicit Light/Deep preparation closes it and wake reloads
the desired mode. Tests cover actual adapter taps, both touch rotations, shared
Wi-Fi/update guards and the controller cleanup/storage fault matrix.
