# Capture-clock camera, AHRS and radio packets

`GET /api/capture` uses the camera's existing authentication and returns one
`multipart/mixed; boundary=biomeframe` response: a length-delimited JSON part,
then its length-delimited JPEG. The handler holds a separate SDK framebuffer;
it never replaces the JPEG copy that an RTSP client may still be transmitting.
Existing RTSP, snapshots, telemetry, light, settings and OTA routes remain.

The `biome.capture.v1` packet identifies the receiver, boot, frame, image size,
`capture_monotonic_us`, `served_monotonic_us`, and `capture_clock: esp_timer`.
The frame ID is `<boot_id>:<capture_monotonic_us>`. The timestamp is the SDK's
first-DMA-buffer capture event; it is not UTC or a claim of simultaneous exposure
of every rolling-shutter row.

AHRS state is selected from a 128-entry PSRAM history, at the last publication
before that capture event. Every sensor retains its own observed uptime,
sample count, validity and age at capture. Missing history is explicit null;
a newer reading is never substituted. At 50 Hz the history spans about 2.56 s.
The history is volatile and contains no images. HTTP status endpoints retain
their current latest-sample semantics.

Wi-Fi/BLE observations keep their actual observation uptimes and stable sample
identities. Observations newer than the capture are excluded. Repeated radio
samples age normally; a camera request does not represent another RF scan.
RSSI-derived ranges remain estimates and do not establish a unique position.

The host validates part lengths, image dimensions, frame identity and common
receiver/boot identity. Tracking intervals use device capture times. A response
service-clock sample and host request interval bound the capture event in the
host's clock without labeling receipt time as capture time. Sensor-to-frame
association does not depend on network arrival order or separate HTTP polls.
The optional BiomeRTE `camera.synchronized_capture` transport consumes this API.

These changes provide synchronized observations. They do not calibrate the
camera-to-IMU mounting transform or implement inertial position integration.
