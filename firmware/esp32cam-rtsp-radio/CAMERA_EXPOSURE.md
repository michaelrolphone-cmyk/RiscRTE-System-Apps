# Camera exposure and gain

Authenticated GET and POST /api/camera/exposure expose driver state and partial updates.
auto_exposure, auto_exposure_dsp and auto_gain are Boolean. exposure_value is 9..1200 sensor driver units, exposure_level -2..2, and gain 0..30. Values are not microseconds or calibrated optical gain. Requests validate every field before application; partial driver failure is explicit, settings are RAM-only, and clients must inspect uncertain outcomes before repeating commands.

The capture clock denotes the first DMA buffer event. Physical exposure start, midpoint, end and rolling-shutter row timing remain unmeasured and are reported as unknown.

This change is based on e3c0add and preserves per-sonar capture-clock range history, camera/AHRS/RF synchronization, the existing gyro calibration API and ultrasonic circuitry/pinout.
