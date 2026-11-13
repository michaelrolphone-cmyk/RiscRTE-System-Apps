# Capture-bound IMU history

Each synchronized camera packet includes up to 32 distinct IMU observations from
the preceding 650 ms in ahrs.ahrs.imu_samples, oldest first. Sampling remains
50 Hz, independently of the 5 Hz camera. Each row preserves the sensor observation
time, sample counter, attitude generation, gravity-bearing acceleration,
bias-corrected gyroscope, quaternion and relative yaw.

The existing ring and compact export buffer use PSRAM. A packet never includes a
snapshot published after camera capture. Samples from other attitude generations
are excluded. IMU or attitude failure stays invalid; allocation failure produces
an empty history, which consumers must treat as insufficient evidence.

Camera exposure controls, sonar histories, pin mapping, radio and light control
remain from the current camera-exposure base.
