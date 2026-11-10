# BiomeRTE Wi-Fi firmware for Waveshare ESP32-S3-Relay-6CH — 2.1.0

This firmware registers the board's physical relays and locally timed output
batches directly with BiomeRTE over Wi-Fi and HTTPS. The board can move between
networks; its device identity and resource name remain stable. It does not need
a USB connection to a Raspberry Pi, a Pi-side serial bridge, or a controller on
the same LAN.

The native provider uses the board's existing relay, RS-485, buzzer and RGB
drivers. Previously configured infrastructure roles keep their existing
credentials and service bindings. The legacy Channel Bus path no longer
advertises or executes this board's physical capabilities. A BiomeRTE-only
installation can use the Wi-Fi setup portal without legacy enrollment.

## Native capabilities

The five capabilities share one board resource, with capacity one. BiomeRTE
fences the resource before granting a lease; individual relays and batches
cannot execute concurrently through separate capability leases. Contract
names describe the operation, while the provider identity describes this board.

| Contract | Inputs | Physical outputs |
| --- | --- | --- |
| `relay.bank:1` | `channel`: 1–6; `on`: boolean | CH1–6 on GPIO1, 2, 41, 42, 45, 46 |
| `device.outputs.batch:1` | `steps`: array of frames | Relay bank, RGB and buzzer |
| `serial.rs485:1` | `baud, parity, stop_bits, tx_hex, rx_max, timeout_ms` | UART1, TX17/RX18 |
| `sound.buzzer:1` | `frequency_hz, duration_ms` | Buzzer on GPIO21 |
| `light.rgb:1` | `red, green, blue`: 0–255 | WS2812 on GPIO38 |

The native contract definitions are in `biomerte-capabilities.json`. Their
outputs are the board driver's direct result fields; there is no legacy
`output` wrapper. Relay results report commanded state and
`contact_feedback: false`: the board does not measure physical contacts.
Channels are numbered by the board. RC-car directions must be assigned from
the actual controller wiring; firmware does not guess that mapping.

### Batches

Each frame may contain `on` and `off` arrays of relay channels, `rgb`
as three bytes, `tone_hz` (zero stops the buzzer), and `wait_ms` after
applying the frame. Omitted outputs retain their state. The runner validates
the entire batch before I/O and applies requested off transitions before on
transitions. Timing runs on the ESP32 after one claimed BiomeRTE operation.

Existing bounds are 1–32 frames, 3,072 input bytes and 60,000 ms total waits.
On successful completion, final output states persist. Results include
`steps_completed`, `elapsed_ms`, per-frame `applied_at_ms` and
`relay_masks`, and final output state. A fence cancels an active batch and
clears its touched outputs before acknowledging new ownership. It does not
change unrelated channels.

A command to release all six relays and stop the buzzer has this input:

```json
{"steps":[{"off":[1,2,3,4,5,6],"tone_hz":0}]}
```

RS-485 retains its 1,200–115,200 baud range, none/even/odd parity, one or two
stop bits, 256-byte transmit and receive bounds, and 10–2,000 ms exchange
budget. It returns raw hexadecimal bytes. Buzzer bounds remain 100–10,000 Hz
and 1–5,000 ms. All relays and the buzzer start off on boot.

## Wi-Fi configuration and pairing

Use a 2.4 GHz Wi-Fi network with DNS and HTTPS access to the public gateway.
The first TLS connection also needs SNTP time sampling; the firmware uses
`pool.ntp.org` while UTC is unknown. A network that blocks time sampling can
prevent the initial pairing connection.

The existing captive setup portal remains at http://192.168.4.1 on the
`RiscRTE-Setup` access point (password `RiscRTE-Setup`). For an already configured board, release BOOT after normal startup, then hold
BOOT for three seconds to request setup. The firmware waits for active HTTP
and physical output work to quiesce before starting the portal. Holding BOOT
during reset selects the chip's ROM download mode instead.

Enter the Wi-Fi network and password. Choose the setup document for the
installation that is actually on the board:

### Preserving an existing 1.0.4 installation

For an already enrolled board, make a **private** setup document using the
current service credential from its existing private pairing file:

```json
{
  "format": "riscrte-setup/1",
  "credentials": {
    "<existing service credential_ref>": "<current private service credential>"
  },
  "biomerte": {
    "url": "https://biomerte-controller.cantikash.chatgpt.site/api/provider/call",
    "site": "f45540bf9910412680734d0613e84460",
    "realm": "site"
  }
}
```

Replace the two placeholders with the existing service credential reference
and its current value. Import this document through the local portal and
leave the existing role selection unchanged. Omit `policy` and relay
configuration: the board retains its saved infrastructure bindings, identity,
credentials and execution custody. Reimporting an old full policy can invoke
relay reconfiguration, which is unnecessary for this migration.

If legacy enrollment has not finished, use its full current private
`riscrte-setup/1` pairing document and add the same top-level `biomerte`
object. The existing bootstrap and custody checks still apply.

Keep either private document private; only the `biomerte` object is public.
The public native-only file is rejected when an existing participant, legacy
credentials, bootstrap data or pending custody are present. Existing
authorization checks for changing paired settings remain in force.

### Fresh BiomeRTE-only installation

For a board with no legacy enrollment or pending custody, import
`biomerte-controller-setup.json`. It is filled in for the existing public
controller and pinned to its site ID. To use another controller, start with
`biomerte-setup.example.json`:

```json
{
  "format": "biomerte-relay-setup/1",
  "biomerte": {
    "url": "https://your-controller.example/api/provider/call",
    "realm": "site"
  }
}
```

Optional `site` pins a specific controller site and optional `resource` names
the board resource. When omitted, enrollment supplies the site and the
resource defaults to `relay6ch.<device-MAC>`. These public setup documents
contain no owner token or private device key. Wi-Fi credentials remain in the
existing local setup journal. Both setup forms save that journal durably
before the native client adopts the configuration.

### Verify the device identity and approve pairing

After Wi-Fi connects, the board generates an RSA-2048 identity and stores it
in its own NVS namespace. It exports only the public key and fingerprint.
The gateway pairing endpoints create a candidate and accept proof of private
key possession; an owner must approve that exact fingerprint through
BiomeRTE's existing `pair.approve` operation. An unapproved candidate never
registers ready capability offers.

To read the generated identity over Wi-Fi, first let the saved setup connect
to the network and create its pairing candidate. Then hold BOOT for three
seconds after normal startup, reconnect to `RiscRTE-Setup`, open
http://192.168.4.1 and expand **BiomeRTE device identity**. Copy its exact
fingerprint and resource. Opening the portal before the first network
connection may show that key generation has not happened yet.

Inspect the identity without resubmitting credentials. The physical recovery
portal exits after 120 seconds with no active client, then rejoins the saved
network. Switch to an internet-connected owner session and approve only the
fingerprint you read from that board. The firmware retries pairing finish
automatically; its backoff can approach five minutes.

Grant the verified device only these scopes, substituting its exact resource:

```text
cap.offer
provider.poll
cap.fenced
provider.claim
provider.result
cap.status
offer:relay.bank:1
offer:device.outputs.batch:1
offer:serial.rs485:1
offer:sound.buzzer:1
offer:light.rgb:1
resource:relay6ch.<device-MAC>
```

An owner registers the five versioned contracts from
`biomerte-capabilities.json` with `cap.define`. The firmware then publishes
its own five offers after enrollment. Keep registration and physical readiness
separate: defining contracts alone does not make a board available.

## Execution and recovery

Signed native requests use RSA PKCS#1 v1.5 with SHA-256. The actor is the
SHA-256 fingerprint of the public key's DER SubjectPublicKeyInfo. Requests
contain the exact site, realm, request ID, operation and arguments; the native
controller validates signatures, current trust and scoped authority.

Before physical I/O, the firmware validates the capability inputs, persists
the resource fence and operation intent, and obtains `provider.claim`.
It persists the result before sending `provider.result`. A reset with
an unfinished durable intent yields an uncertain outcome instead of replaying
the effect. Lost claim or result acknowledgments are reconciled by operation
identity. Reconnection cannot restart an already attempted batch.

The firmware shares the existing HTTPS driver with the infrastructure agent
through explicit scheduling. Each HTTP owner finishes cleanup before the other
opens a request. Pairing, connection retries and native result reporting do
not depend on a serial link.

Configuration, identity and execution state are stored in native NVS.
Corrupt existing state fails closed; boot does not erase or format NVS.
The existing brownout detector and watchdog remain enabled.

## Controller gateway

The companion BiomeRTE change adds:

- `POST /api/provider/call` for signed provider operations;
- `POST /api/provider/pair/begin` and `/finish` for candidate enrollment;
- a controller-side adapter that forwards the original signed JSON without
  substituting owner authorization;
- optional `provider.poll limit` for the ESP32's bounded response buffer.

Pair approval and contract definition remain owner operations. The gateway
does not cache a successful authorization across revocation: every forwarded
provider request passes through current native signature and scope checks.

## Build, update and qualification

Pinned runtime:
`michaelrolphone-cmyk/RiscRTE@2747b5907b1af06882e90a5a8d7ca355b34a7abf`.

Pinned toolchain:
`espressif/idf:v6.1@sha256:81893c71bb5e570088901f21def8684c25cd2a9020281bd01b843a7655edb18c`.

Set `RISC_RUNTIME_ROOT` to that runtime checkout, source ESP-IDF 6.1, run
`python scripts/generate_relay6ch.py`, then configure and build:

```sh
idf.py -C firmware/waveshare-relay6ch -B relay-build \
  -D RISC_RUNTIME_ROOT="$RISC_RUNTIME_ROOT" reconfigure
cmake --build relay-build -- -j1
python scripts/package_relay6ch.py --build relay-build \
  --runtime "$RISC_RUNTIME_ROOT" --output delivery
```

Host gates are `scripts/test_infrastructure.sh`,
`scripts/test_biome_integration.sh`, `scripts/test_relay6ch.sh` and
`scripts/test_biome_node.sh`; run the latter two with and without `SANITIZE=1`. CI also verifies that regenerated factory
policy and provider pins match the committed files before compiling the
ESP32-S3 target.

The product uses 8 MB flash, DIO at 40 MHz, internal RAM, and no PSRAM. The
application-only image is installed at `0x50000` using the established
partition layout and preserves saved settings. The first-install image at
offset zero replaces saved data and is not a preserving upgrade.

The existing 1.0.4 firmware has no OTA updater. Publishing this release does
not install it on a running board. This release does not add an unverified
network flashing path. Installation must use a supported bootloader or
installer path for the board; normal operation and registration use Wi-Fi.

Compiled images and host tests provide software evidence. Physical relay
motion, RS-485, waveform timing, memory under load and power-interruption
behavior require qualification on the actual board. Release manifests report
`hardware_tested: false` until such evidence is recorded.

Hardware reference: https://docs.waveshare.com/ESP32-S3-Relay-6CH


## Version 2.1.0 streaming release

Adds `device.outputs.stream:1` alongside the existing discrete contracts. Desired states are checked for authority, fence generation, epoch, sequence and freshness by the provider's local output task. Transport loss or expired driving intent applies neutral locally; received and applied acknowledgements remain separate from physical contact feedback. The gateway must provide reachable persistent stream hosting before live attachment.

The release workflow builds and publishes the board-specific no-PSRAM image, flash manifest and checksums. Physical installation and timing qualification remain separate from the successful host and target builds.
