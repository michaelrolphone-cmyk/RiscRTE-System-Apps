# Actual rear adapter allocation and reset-reason regression

Run after materialization installs the pinned dependencies:

```sh
python firmware/esp32cam-rear-lidar/test_lidar_adapter.py --prepared /tmp/rear-camera-build
python firmware/esp32cam-rear-lidar/test_lidar_adapter.py --prepared /tmp/rear-camera-build --source /tmp/rear-camera-build/src/radio_telemetry.cpp
```

The host translation unit includes the selected production radio_telemetry.cpp
without extracting or rewriting initialize(). It uses the prepared, hash-guarded
SparkFun VL53L5CX 1.0.3 declarations and data structures, and real ArduinoJson.
The clock, GPIO, heap allocator, task creation, HTTP request boundary and sensor
hardware operations are deterministic fakes. The shared BNO test directory
supplies only generic WebServer, FreeRTOS/task and esp_timer boundary stubs.
No task, sensor, board endpoint or real network connection is started.

Five groups cover:

- Configuration retained while results allocation repeatedly fails, then recovery.
- Results retained while configuration allocation repeatedly fails, then recovery.
- Both allocations failing, later recovery, and hardware-init failure/retry using
  the same successfully allocated pair.
- Actual begin/registered HTTP handlers: once-only cached boot reason, repeated
  begin, authenticated rear-only fields, unchanged alternate status content,
  both 64-zone snapshots, and absence of diagnostic fields in frame metadata.
- Raw reset enum and stable label mapping, including an unrecognized code.

The asymmetric cases keep the other head ready and verify its allocations,
sample identity, complete published ranges and communications-enable state.
Counts/pointer assertions detect both lost retry and leaked retained results.
The unchanged fixture is the exact .10/.11/.16 adapter, SHA256
46d924f6dd1a3cfaa293b4cada60b675f64928a3b386128d17354429be04abf8.
The same test source must fail for each original asymmetric defect with the
specific behavior assertion; a compiler failure or unrelated crash is not a
negative-control pass.

The existing test_lidar_driver.py separately exercises real low-level I/O/ULD
read and initialization deadlines. These adapter tests do not simulate
electrical faults, prove that an allocation failure occurred on the live board,
or identify the cause of earlier resets. Reset telemetry describes only the
boot that exposes it. Optional --asan is for hosts with working sanitizer
support; an unavailable sanitizer must not be reported as a pass.
