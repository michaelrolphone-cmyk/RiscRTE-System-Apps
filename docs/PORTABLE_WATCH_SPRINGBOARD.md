# Explicit native Watch Springboard

`build_portable_springboard.py --watch-native-ui` selects the shared Watch color
presentation at the capability's native dimensions. It does not select paper
safe-viewport or portrait rotation. Unknown refresh metadata is accepted only
for this explicit Watch profile; known rates below 20 Hz, retaining displays,
and known presentation latency above 50 ms remain on the static fallback.
No refresh-rate or physical responsiveness claim follows from this selection.

Add `--time-profile watch-native-time --native-time-runtime-repo PATH` for the
same readonly native UTC / selected-IANA-zone owner used by other native-time
apps. Display rotation is zero. Runtime's canonical SDK is read at the pinned
revision, not inferred from a directory name. The app requires
`runtime.realtime@1` and namespace-1 `storage.key-value@1`; it has no clock seed
or external RTC grant. Missing timezone preference means UTC; invalid/unreadable
preference or UNSET native time displays no invented current time. Home and
Settings retain their separate initialization/editing authority.

No alarm service is required. An explicit alarm client must also select the
pinned tagged API2 Utilities SDK. Paper, resident-shell and diagnostic
performance selections cannot be combined with this Watch profile. Existing
X4 native-time profile requirements are unchanged.

The builder emits exact flags, native-time source/SDK hashes, manifest and
`watch-native-app.json` admission evidence. Firmware composition must provide
matching grants and qualify the entire selected cohort separately.

Host qualification:

```
python scripts/test_springboard_watch_profile.py --runtime PATH --output build/watch
ASAN_OPTIONS=detect_leaks=0 python scripts/test_springboard_watch_profile.py \
  --runtime PATH --output build/watch-sanitized --sanitize
```

The test drives the real app, raster adapter, touch controller and native time
owner across 240x240, 466x466, 410x502 and 240x320 capability surfaces. It covers
known/unknown/slow/retaining metadata; repeated, cancelled and interrupted touch
flows; delayed frame completion; native UTC/Denver, UNSET and preference IO.
Providers are host models. Target ELF validation is not physical qualification.
