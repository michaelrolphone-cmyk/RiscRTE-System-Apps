# Explicit native time profiles for foreground System Apps

These local-only profiles select the readonly native UTC clock projected through
the current selected IANA timezone, plus tagged `alarm.service@2`. They are
available in the existing Springboard, File Browser and Wi-Fi Settings builders
only with `--time-profile x4-native-time`. Their independent versions are
Springboard **1.7.2**, File Browser **1.5.4** and Wi-Fi Settings **1.1.7**. Default
Watch/raw-paper source versions remain 1.7.1, 1.5.3 and 1.1.6 respectively.

Version selection was checked against public System Apps main on 2026-10-07:
Springboard 1.5.0, native File Browser 1.4.1 and Wi-Fi Settings 1.1.4. These new
versions were separately reserved for the native integration; no tag, release,
remote branch, product source pin or deployed bundle is published by this work.

## Build selection

All three builders accept these explicit common arguments:

```sh
--time-profile x4-native-time \
--native-time-runtime-repo /path/to/Runtime \
--tagged-alarm-utilities /path/to/Utilities \
--alarm-client --quick-actions --home-app default.elf
```

The profile selects `--display-rotation 90` and navigation. It rejects Watch RTC
policies, legacy File Browser Quick Controls, nonportrait orientation and
Springboard/Wi-Fi Nova selection. Quick Actions is optional; existing separate
radio controls remain explicitly selected. The selected profile does not
initialize native time or write the timezone preference. A missing record uses
the checked Reader UTC default; a corrupt or unreadable record is unavailable.

Use `scripts/build_portable_springboard.py` with the final admitted deployment
`--catalog`, not the empty development catalog used in qualification. Use
`scripts/build_portable_file_browser.py` with the product's actual storage
capability, exact instance(s), handlers and return destination. The test selection
is `--storage-capability storage.volume --storage-instance 11`. Use
`scripts/build_portable_wifi.py --wifi-instance 15 --return-app springboard.elf`
when those are the product's exact radio and Back bindings.

Springboard's existing page toolbar and Quick Actions share the checked native
callback. File Browser and Wi-Fi Settings retain their current page layouts;
their existing Quick Actions clock uses that same native source.

## SDK, authority and receipts

The staged include tree replaces its Runtime SDK headers with exact Git-object
bytes from Runtime `30dcec5ce6ce33223f2b203a2399283e1f758567`. The two alarm headers
come from Utilities `637e13b0bce62ad49b756bec2468a6271d163fc7`. Both licenses and
header digests are staged with the build. The repository's compatibility header
and all default builder headers remain unchanged.

Compile selections are `PORTABLE_NATIVE_TIME_TOOLBAR`,
`PORTABLE_NATIVE_CUSTODY_FENCE` and `ALARM_SERVICE_TAGGED_V2`. The reusable
`PortableNativeTimeSource.c` is linked separately with `PortableRealtimeClient.c`,
`PortableTimeZone.c`, `PortableTimeZoneCatalog.c` and
`PortableTimeZonePreference.c`.

The standard selected grants are:

- `display.output@1`, `input.touch.raw@1`, `input.navigation@1`, `board.battery@1`,
  and tagged `alarm.service@2`, each instance 0
- readonly `runtime.realtime@1`, instance 0
- `storage.key-value@1`, namespace 1, for timezone and the existing Quick
  preferences; only Quick actions can write their existing preference keys
- File Browser's selected volume and optional handler grants
- Wi-Fi Settings' exact `net.wifi@1` instance plus credentials KV namespace 6
- Explicit Quick radio grants only if requested

No `runtime.realtime-control`, `rtc.clock` or alarm API1 grant remains in native
manifests. Builders emit requirements and integration receipts; they do not
apply grants. The app build record carries each actual instance binding and
complete source hashes. `x4-native-app.json` carries app/version, clean source
revision, System/Runtime/Utilities revisions, alarm API2, native time policy,
ELF hash/size, the exact manifest requirements and all four compiled SDK header
digests. A dirty build is explicitly marked and is not a clean package receipt.

Native finalization closes app-owned File/Wi-Fi/RF/audio resources in the
established order before tearing down adapter resources. Any refusal or retained
result fences the invocation and prevents later I/O, cleanup or frees.

## Qualification

```sh
python scripts/test_native_system_apps.py \
  --runtime /path/to/Runtime --utilities /path/to/Utilities
python scripts/test_native_owned_cleanup.py --runtime /path/to/Runtime
python -m unittest discover -s tests -p 'test_native_system_build_profiles.py'
```

The System Apps runner builds all three real applications with the production
adapter, native owner/helpers and tagged alarm SDK. For each application,
12 cases run normally and under ASan/UBSan: Home success/refusal, explicit UTC,
selected timezone, missing timezone default, unset native time, Quick clock and
DND action, live app-owned finalization, refused finalization, KV custody loss,
native release refusal and tagged alarm retention. Provider doubles reject RTC
or control acquisition and assert no later provider calls or frees after fencing.
The lifecycle runner adds 10 normal/sanitized RF/audio ordering/refusal/retained
cases, including hooks returning true after they retain custody.

Six native Xtensa links (Quick on/off) pass real loader and import/export gates.
Six complete legacy Watch/raw-paper ELFs and manifests must compare byte-for-byte
with System Apps `d305e6b718f51d42ffcf6339e05ce4b56398f68c`. The output evidence
records the exact source revision and target identities. These are host and target
link checks; no hardware execution or firmware/BIN qualification is claimed.
