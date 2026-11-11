# Four HC-SR04 sensors — FORIOT ESP32-S3 CAM N16R8
Firmware 1.0-radio.8. This is the dual-USB, 20-pins-per-side S3 CAM profile
already used by the rover, not the AI Thinker ESP32-CAM.

## Wiring
View the camera side with the antenna at the top and USB sockets at the bottom.
Count each header from top (1) to bottom (20). Match the printed IO labels;
positions refer to the matching MB0184/FORIOT layout.

| Sensor ID | TRIG GPIO | Physical position | ECHO GPIO | Physical position |
| --- | --- | --- | --- | --- |
| front | IO1 | right 3 | IO47 | right 16 |
| left | IO2 | right 4 | IO3 | left 12 |
| right | IO14 | left 19 | IO45 | right 14 |
| rear | IO21 | right 17 | IO46 | left 13 |

The direction names are mounting conventions. They do not establish measured
mount angles or sensor extrinsics; consumers must calibrate those separately.
IO2 may also drive the board's status LED; its blinking during ranging is expected.
This profile does not reassign the camera, AHRS, RGB light, SD or either USB port.

All four VCC pins -> regulated 5V (board 5V at left 20, provided that rail has
adequate current); all four GND pins -> common ground (board GND at right 20).
Budget at least 60mA for the four modules in addition to camera-board demand.
Keep motor power noise off the sensor supply. Do not power HC-SR04 from 3V3.

**Never connect a standard HC-SR04 ECHO directly to an ESP32 GPIO.**
For EACH ECHO: sensor ECHO -> 1k ohm resistor -> junction -> assigned ESP input.
Connect a 1.8k ohm resistor from that junction to GND. At a 5V echo this yields
about 3.21V. Use four independent dividers (four 1k + four 1.8k resistors).
TRIG is a 3.3V output to the module's TTL trigger input. If a particular clone
does not accept 3.3V triggers, use a suitable 3.3V-to-5V buffer; never feed 5V
back to an ESP output.

**IO3, IO45 and IO46 are boot-strapping inputs.** Their ECHO junctions must
remain LOW during board power-up/reset. The divider's lower resistor gives a
physical pull-down, but cannot override an actively HIGH sensor output.
Power the sensors with the board and avoid resetting/flashing while a module
is asserting ECHO. Disconnect the three strap ECHO wires (or power the modules
off with their grounds still connected) for recovery/flashing if necessary.
A stuck-HIGH module may prevent normal boot. Firmware cannot correct reset-time
strap levels. GPIO46 is configured only as an ECHO input.
For a design that must recover automatically even with faulty/independently
powered sensors, add reset-controlled ECHO isolation or a separate ranging MCU.

AHRS stays: 3V3 left 1; GND right 20; SDA IO41 right 6; SCL IO42 right 5.

## Consumer contract
Authenticated GET /api/ultrasonic returns the existing receiver/boot/time envelope
with type "ultrasonic" and an ultrasonic object. GET /api/telemetry includes
ultrasonic alongside ahrs. Existing AHRS endpoints/packet schema are preserved.
UDP sends separate type "ultrasonic" packets to the same configured udp_host and
udp_port, at most once every 260ms. Empty udp_host disables network publication.
Each packet carries the latest cache, not a simultaneous four-sensor scan.
The bundled telemetry_receiver.py validates, logs and displays these packets.

ultrasonic.schema = "hcsr04.v1"; sensors is a fixed four-element array with
id, sequence, valid, status, distance_m, echo_us, observed_uptime_ms and age_ms.
Full HTTP output adds trig_gpio/echo_gpio; UDP omits these repeated pin fields.
worker_ready, paused, trigger_interval_ms, stale_after_ms and sound_speed_m_s
are explicit. Each sequence advances on an attempt, including timeout.
Consumers must add transit/cache time to age_ms and reject samples over 1000ms
old. Repeated publication is not a new measurement. boot_id distinguishes reset.
A disconnected sensor cannot be distinguished from no returned echo; there is
no fabricated presence flag.

The worker triggers exactly one module every >=65ms (about 3.85Hz per sensor)
with a 12us pulse. GPIO edge interrupts timestamp ECHO; a separate low-priority
task waits up to 30ms while yielding, never in the camera service loop.
Measured distance uses a fixed 340m/s sound speed: echo_us * 0.000170 metres.
Valid range is approximately 0.02–4m. No temperature compensation, certified
obstacle avoidance or angular localization is claimed.
timeout, echo_stuck_high, out_of_range, paused, stale, not_sampled and
worker_unavailable all publish valid=false and null distance/echo values.
OTA pauses ranging and invalidates previous samples.

## Qualification
Build with the existing prepare.py --build command. CI covers all existing
radio/light/AHRS tests, receiver tests and new range/freshness/packet tests.
After flashing and wiring: check all four channels against a flat target,
unplug each ECHO in turn (null/timeout), test a blocked/HIGH input, verify no
reset during streaming, and measure cross-talk with the final sensor mounts.
Check boot/reset with the sensors powered. Compile tests do not establish
physical wiring, voltage levels, distance accuracy or video throughput.

## References
- Manufacturer matching board pinout: https://docs.keyestudio.com/projects/MB0184/en/latest/docs/MB0184%20ESP32-S3%20CAM%20Development%20Board.html
- HC-SR04 module sheet: https://cdn.sparkfun.com/datasheets/Sensors/Proximity/HCSR04.pdf
- ESP32-S3 strapping and input limits: https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf
