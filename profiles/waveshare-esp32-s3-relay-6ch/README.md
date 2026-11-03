# Waveshare ESP32-S3-Relay-6CH RiscRTE agent 1.0.0

Dedicated static ESP-IDF 6.1 product for the ESP32-S3 relay board. It links the
same infrastructure agent and enrollment portal used by the camera product,
without the camera's PSRAM-dependent ELF loader and paired-bank store. It does
not require PSRAM. The 8 MB/DIO/40 MHz image also fits larger-flash variants.

The five concurrent, dynamically allocated roles remain Relay, Explorer,
Sentinel, Reconciler and Continuity. Eligibility still requires each role's
local credentials and service bindings; flashing does not grant those scopes.
Shared relay credentials, leases, fencing, drain and handoff rules are unchanged.
Four additional physical capabilities remain eligible independently of staffing
assignments, under ordinary participant/session/execution grants.

| Capability | Operation | Board resources |
| --- | --- | --- |
| `relay.bank@1` | `set(channel:1..6,on:boolean)` | CH1..6: GPIO1,2,41,42,45,46; HIGH on |
| `serial.rs485@1` | `exchange(baud,parity,stop_bits,tx_hex,rx_max,timeout_ms)` | UART1 TX17/RX18; automatic direction |
| `sound.buzzer@1` | `tone(frequency_hz,duration_ms)` | GPIO21; 100..10000 Hz; at most 5 seconds |
| `light.rgb@1` | `set(red,green,blue)` | GPIO38 WS2812; 8-bit RGB |

RS-485 supports 1200..115200 baud, 8 data bits, none/even/odd parity and 1/2 stop
bits. Each exchange transmits at most 256 bytes and receives at most 256 bytes
within a 10..2000 ms total budget. It returns raw hex; it does not claim Modbus
protocol validation. Relays return commanded state, not physical contact
feedback. All relay outputs and the buzzer start off; saved commands are never
restored on boot. There is one serial transaction/job at a time.

Physical execution requires validated contract/provider/package pins, a fresh
start grant, its exact input digest/effect class, and remaining lease time.
A durable local intent and acknowledged remote effect record precede I/O.
Reboot or uncertain persistence reconciles the old attempt instead of replaying
physical I/O. An uncertain UART transmission remains an unknown effect.

Configuration and the CRC-framed agent journal use native NVS atomic blob
writes. Corrupt/missing existing journal state fails closed; boot never formats
NVS. Brownout detection and a 30-second task watchdog remain enabled. Network
backoff/reconnect and independent relay-service recovery use the existing
agent. Serial logs report lifecycle state, reason, reset reason and RAM floors.

First boot exposes `RiscRTE-Setup` (password `RiscRTE-Setup`) with the existing
captive portal at http://192.168.4.1. Supply a fresh private infrastructure
pairing file. The first four shared contracts must match the established camera
cohort; the firmware substitutes its own immutable eight-provider and memory
pins while preserving the pairing's endpoints and credentials. Use a separate
identity/credential for each device. Hardware definitions are in `catalog.json`.
The catalog package is a deterministic source archive pinned into the image.

Build: set `RISC_RUNTIME_ROOT` to the pinned runtime checkout and source the
pinned ESP-IDF 6.1 environment. Run `python scripts/generate_relay6ch.py`, then
`idf.py -C firmware/waveshare-relay6ch -B relay-build -D RISC_RUNTIME_ROOT=$RISC_RUNTIME_ROOT build`.
Use single-job compilation (`cmake --build relay-build -- -j1`) for qualification.
`bash scripts/test_relay6ch.sh` exercises input limits, hardware-role independence,
remote intent acknowledgment, once-per-boot execution, reboot fences and policy.

The release contains a first-install full image at offset 0 and a preserving
application-only image at 0x50000. A first-install image clears NVS; subsequent
application-only updates preserve settings when this exact partition layout is
already installed. No remote OTA agent or dynamic ELF installation is provided
by this static product. Target builds and host tests are software evidence;
physical relay/RS-485/waveform, memory-under-load and power-interruption tests
remain unqualified until run on the board. No device is flashed by publication.

Primary pin/schematic reference: https://docs.waveshare.com/ESP32-S3-Relay-6CH
