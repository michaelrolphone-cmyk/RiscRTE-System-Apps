# Read-only UTC suffix for rtc.clock@2

`sdk/app/RiscRtcUtcV1.h` defines an optional tagged extension after the unchanged
calendar API prefix. Existing clients and default provider builds remain valid.
There is still one driver descriptor and one provided capability.

Consumers copy-bind the suffix, check its status, and request a copied UTC
snapshot. A successful read supplies epoch seconds. A failed read leaves the
entire snapshot unchanged. A recognized malformed suffix is not equivalent to
an absent suffix: consumers must not substitute a potentially stale clock.

The provider owns calendar validation and its explicit basis. The reusable
`sdk/driver/RiscRtcUtcCalendarV1.h` codec converts a validated 2000–2099 calendar
using a supplied offset. It does not infer a timezone from a chip, device or
application. It performs no writes, native clock seeding or alarm acknowledgement.

Provider recipes opt in with `RTC_UTC_OFFSET_SECONDS`. Without that definition,
the two qualified reference providers retain their original prefix-only target
bytes. The selected +28800-second recipes preserve existing stored calendar
meaning, including UTC946656000 at the low boundary and UTC4102415999 at the
high boundary. No stored clock or fingerprint data is migrated.

`scripts/build_rtc_utc_provider.py` accepts the provider source, original
manifest, explicit offset/version and include roots. It builds a separate
successor package twice, checks identical bytes, and runs the selected Runtime
structural validator. Original product recipes and their pinned driver images
are not silently changed. Package selection belongs to product composition.

The qualified PCF8563 and PCF85063A implementations validate this reusable
contract; they are not a finite device list in consumer code. Endpoint tests,
target structure/repeat and metadata admission do not claim physical RTC or
native-instruction execution.
