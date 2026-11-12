# Camera-clock sonar mapping observations

Firmware 1.0-radio.10-sonar-mapping adds causal per-transducer history to synchronized capture packets. Samples must be measured and completed before image capture. Boot identity and individual 64-bit esp_timer measurement timestamps are preserved. Independent telemetry and four trigger/echo pin pairs remain.

Full ESP32-S3 N16R8 build passed; 26 receiver tests and five C++ model binaries passed. Firmware SHA256: 86975d457474fd7f8980df61ac22bdfa4ece192700670d1e9efadccbff6e6ec2. Not flashed; preserve the independent driving experiment.
