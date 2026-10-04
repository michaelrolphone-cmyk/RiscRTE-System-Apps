# Portable Wi-Fi Settings 1.1.0

`PORTABLE_WIFI_SETTINGS_APP` opts the existing `Apps/wifi_settings.c` into a
shared application/client implementation. Without the define, the Reader
firmware-selector implementation is unchanged. No network stack, credential
entry screen, or settings policy is added to firmware. The renderer uses the
existing Settings fonts, cyan-on-black palette, and shared display/touch adapter.

## User workflow

- Enter a network name, or scan up to sixteen nearby networks. Hidden networks
  use manual entry; unsupported authentication is visible but cannot be selected.
- The application keyboard offers all 95 printable ASCII characters in three
  bounded pages. SSID is at most 32 bytes, password at most 63 characters. A
  scanned non-ASCII SSID is copied as bytes; the compact font substitutes `?` for
  unsupported glyphs. There is no clipboard, password log, or firmware keyboard.
- Password entry is masked. Done updates a RAM draft; Back discards the editor
  draft. Done with an empty password does not silently choose an open network.
  Choose Security → Open explicitly, or select an open scan result.
- Connect is explicit. Accepted association is shown as Connecting, never as
  Connected. IPv4/radio status establishes connection; timeout is 30 seconds.
  Cancel, Disconnect and retry are available. A scan times out after 15 seconds.
- Save explicitly persists one profile; merely editing, scanning or connecting
  does not. Loading a profile never automatically connects.
- Forget asks for confirmation, logically removes the profile and clears RAM.
  Storage is plaintext, not an encrypted vault. Logical overwriting cannot
  guarantee physical flash erasure; the help screen states this limitation.

The current app session closes on root Back and idle sleep. This is scoped
lifecycle management, not a claim that all future networking must be foreground
only. Other explicitly granted consumers can load and connect the saved profile
through the reusable client described below. No boot reconnect policy is added.

## Explicit capabilities and lifecycle

The app requires display.output@1, input.touch.raw@1, net.wifi@1 with the checked
management suffix, and namespace-6 storage.key-value@1. The Watch deployment
selects Wi-Fi instance 15, storage instance 6, navigation, alarm overlay and its
existing hybrid idle-sleep hook. An older Wi-Fi table is not called past its
reported size and cannot start an uncleanable session. Missing storage permits a
temporary connection but cannot produce a Saved confirmation.

`WIFI_RETURN_APP` is an app-owned root-only handoff. `PORTABLE_RETURN_APP` is a
compile error because that adapter policy would consume nested Back. Password,
SSID, scan, privacy and Forget screens own Back before root exit. Input needs a
neutral frame after page switches and wake; held or wake input cannot save,
connect or exit. All operations are serialized on the app owner task.

Before idle sleep, `portable_wifi_suspend()` calls checked disconnect (which
cancels/drains scan too) then releases the Wi-Fi grant. A failed drain/release
refuses sleep and preserves the interactive app for cleanup retry. On ordinary
wake/refusal, `portable_wifi_resume()` reacquires the grant, preserves the RAM
editor, and does not reconnect. A native-retained result bypasses resume,
cleanup, rendering and provider I/O; the existing Runtime retention barrier owns
that case. Alarm overlays retain the exact draft/frame through dismiss. Root
exit also requires checked disconnect and grant release. Failed normal finalizer
cleanup retains the mapped invocation rather than claim that native ownership
was released. All local credential buffers are volatile-wiped on normal close. Every
storage-backed alarm safe point actively reconciles radio health, including
completed scans that still own native state. Uncertain cleanup blocks alarm
pumps and app Save/Forget without revoking their storage grants. If failure
appears during an overlay, bounded output-only stop and frame restore return
control for cleanup retry; no occurrence is acknowledged by this pause. Fatal
paths drain Wi-Fi before service cleanup or retain the invocation.

## Five-key interrupted-write protocol

`PortableWifiCredentials.h` owns the public reusable codec. Namespace 6 uses
exactly five fixed keys. The app grant covers the namespace; the eight-key
limit belongs to bound-provider key maps, not to an app quota:

- wifi.a0, wifi.a1: two 64-byte chunks for slot A
- wifi.b0, wifi.b1: two 64-byte chunks for slot B
- wifi.commit: one 32-byte selector or Forget tombstone

A record has a version, generation, explicit lengths, canonical zero padding,
chunk CRC32s, combined checksum and selector checksum. These detect accidental
corruption; they do not authenticate or encrypt data. Save writes and verifies
the inactive slot, then writes and verifies the selector. The selected complete
old slot is untouched before commit. No orphan or older slot is ever used as a
fallback when selector/chunks are missing or invalid, preventing resurrection
after Forget. The protocol assumes serialized single-writer, per-key backend
replacement. It cannot manufacture a transaction or backend durability promise.

A put IO result can mean persisted or not persisted; exact readback determines
confirmation. Uncertain readback yields Unconfirmed, with explicit retry. Forget
commits and verifies the tombstone first, then overwrites and verifies all four
chunks. If wiping fails, the profile stays logically forgotten and the UI warns
that old storage bytes may remain. Every temporary encoding/decoding buffer is
volatile-wiped. Physical wear-leveling copies may remain even after success.

## Saved-profile consumer contract (OTA / App Store)

`PortableWifiSavedNetwork.h` exposes a bounded client function using this same
codec. Consumers must be explicitly granted storage namespace 6 and net.wifi
instance 15 by deployment policy, validate their runtime grants, then call
`portable_wifi_saved_network_connect(storage, wifi)`. The helper does not acquire
extra authority, invent a namespace, enumerate records, or own boot policy.
Only a valid saved profile and a DOWN station permit one connect request.
Non-DOWN state is Busy; no existing connection is disconnected. STARTED means
accepted, not online: the consumer must poll status/addresses with its own bounded
timeout, maintain foreground/owner-task rules, and drain/release before sleep or
exit. Consumers must not access the storage helper while earlier native cleanup
is uncertain; DOWN alone is insufficient. A failed connect needs immediate
checked cleanup before any storage-backed service call. The provider must synchronously copy connect strings. The helper wipes its
local credential copy on every outcome and never logs it. A consumer with no
namespace-6 grant cannot read the profile; ordinary Settings or unrelated apps
do not inherit Wi-Fi credential authority.

## Build and verification

- `python scripts/test_portable_wifi.py`: production controller/renderer/adapter
  with fake providers, both touch orientations, plain C11 and ASan/UBSan; codec
  interruption/failure suite and reusable saved-consumer contract tests.
- `python scripts/build_portable_wifi.py --alarm-client --navigation --full-frames --wifi-instance 15`:
  standalone Xtensa ELF, structural loader validation, strict import/export
  inventory, font notices and SHA-256 source/build records.
- Existing aggregate legacy and portable suites remain applicable. The normal
  native builder still compiles the unchanged Reader path.

Fixtures never operate a host network or use actual credentials. They cover
connect/error/timeout/cancel/retry, scan corruption and cleanup, keyboard bounds,
Back/neutral input, save/Forget, grant denial, all interrupted/uncertain storage
write boundaries, retained alarms, ordinary sleep, native-retained no-late-I/O,
and cleanup failures. Host/ELF validation does not claim real-device RF, flash
persistence, measured idle power, or physical UI qualification.
