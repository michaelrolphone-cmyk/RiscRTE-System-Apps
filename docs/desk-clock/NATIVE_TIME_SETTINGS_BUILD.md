# Opt-in native-time Settings build

`x4-native-time` selects the separate development identity **1.3.7**. The
default and `x4-desk-clock` identities, flags and target linkage remain unchanged.
This profile requires the matching native Settings controller and custody fence;
the builder refuses a source tree missing either implementation.

```sh
NATIVE_APP_CC=/path/to/xtensa-esp32s3-elf-gcc \
python scripts/build_portable_settings.py \
  --settings-profile x4-native-time \
  --native-time-runtime-repo /path/to/RiscRTE \
  --return-app springboard.elf --home-app default.elf \
  --output-dir build/native-time-settings
```

The profile selects portrait paper (`--display-rotation 90`) and navigation.
It rejects rotation 0, Nova UI and the old `--denver`/`--wall-time` RTC policies.
It enables `PORTABLE_SETTINGS_NATIVE_TIME`, `PORTABLE_SETTINGS_TIME_ZONE`,
`PORTABLE_SETTINGS_X4_DESK_CLOCK`, `PORTABLE_NATIVE_CUSTODY_FENCE` and
`PORTABLE_SLEEP_SETTINGS`. Sleep modes remain Light/Deep and the existing six
faces remain available. Optional alarm/QuickActions flags retain their existing
explicit requirements.

## Authority and SDK custody

The application manifest adds `runtime.realtime-control@1`. The controller uses
instance 0 (unique authorized native provider); the manifest's capability/API
schema does not select provider instances. `rtc.clock@2` remains declared for
explicit Save and `storage.key-value@1` remains declared for the existing
namespace-1 preferences. No provider-promotion capability is added.

The builder reads `sdk/app/RiscRuntimeV1.h`, `sdk/app/RiscRealtimeV1.h` and
`LICENSE` directly from local Git objects at Runtime
`602ae9bd618e13407b5b94bcad86cdabc23c99ea`. It does not fetch or use the checkout's
current header files. The complete bundled include tree and relative time
includes are staged only for this profile, then the two canonical headers replace
their bundled counterparts. This avoids quoted includes binding to an old
Runtime prefix. A product Runtime descendant may use identical header bytes;
its product-source identity remains separate from this exact SDK-source pin.

Actual linked helpers are `PortableSetTime.c`, `PortableRealtimeClient.c`,
`PortableTimeZone.c`, `PortableTimeZoneCatalog.c` and
`PortableTimeZonePreference.c`. They are not linked into either older profile.
The existing source hash inventory includes the controller, adapter, helper,
preference, catalog and provenance inputs. Native build records additionally
include `time_policy: native-realtime-iana`, `invocation_retention: true`, the
exact `native_time_runtime_commit`, and SHA-256 for both canonical SDK headers
and the Runtime license. The license bundle retains System Apps' license, font
notices, RTC provenance and time-source notices, and adds timezone provenance,
Runtime's exact license and a source-hashed SDK provenance record.

The timezone catalog contains frozen representative recurring rules. The policy
name is not a claim of full historical IANA coverage. Successful builds still
require the actual ELF32 Xtensa layout, loader validator, import allowlist and
three existing exports. Build flags or a builder unit test alone do not prove
controller behavior, physical clock accuracy or hardware sleep readiness.

## Focused verification

```sh
python -m unittest discover -s tests -p 'test_native_settings_build_profile.py' -v
```

The builder tests check missing/incompatible inputs, immutable SDK selection,
quoted-include staging, source isolation, manifest authority and build-record
custody. Their compiler calls are mocked explicitly; run the combined controller
host tests and a real pinned Xtensa build before claiming integrated behavior.
For regression, build the default, Watch and `x4-desk-clock` commands before and
after changes and compare actual ELF/manifest bytes and recorded build flags.

The native profile also declares `board.battery@1`: startup opens this provider,
so failed acquisition cannot be treated as optional clean unavailability.
`required_grants` records each capability/API and required `instance_id`: KV is 1,
optional Wi-Fi 15 and Bluetooth 16; other instance 0 entries require the deployment's
unique authorized provider/native capability. `grant_count` counts distinct
manifest requirements. `mode_capabilities` records native read/explicit checked
Save, preference-only timezone, Light/Deep preference choices, optional alarm and
Quick controls, and explicitly says `sleep_backend=false`. Selecting this profile
does not enable a Settings sleep backend or install boot grants.
