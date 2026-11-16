"""Host tests for the real UDP receiver; run with Python's unittest discovery."""
import copy
import importlib.util
import json
from pathlib import Path
import select
import signal
import socket
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("telemetry_receiver", ROOT / "tools" / "telemetry_receiver.py")
receiver = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(receiver)


def packet(receiver_id="cam-one"):
    return {"schema":1,"type":"summary","receiver_id":receiver_id,
            "boot_id":"boot-a","uptime_ms":30000,"timestamp_ms":None,"sequence":1,
            "ftm":{"status":"success","distance_m":1.52,"rtt_ns":10,"age_ms":2000,"samples":16}}


def encode(value):
    return json.dumps(value).encode("utf-8")


def record(value=None, ip="192.0.2.1"):
    return receiver.received_record(value if value is not None else packet(), (ip, 9876))


class ParsingTests(unittest.TestCase):
    def test_retired_scan_packets_are_rejected(self):
        for kind in ("wifi","ble"):
            value=packet();value.update(type="observation",radio=kind)
            with self.assertRaises(receiver.PacketError):
                receiver.parse_datagram(encode(value))

    def test_summary_ftm_measured_range(self):
        value = {"schema": 1, "type": "summary", "receiver_id": "cam-one",
                 "uptime_ms": 30000, "sequence": 2, "status": {},
                 "ftm": {"status": "success", "distance_m": 3.42,
                         "rtt_ns": 23, "age_ms": 123, "samples": 16}}
        self.assertEqual(receiver.parse_datagram(encode(value))["ftm"]["distance_m"], 3.42)

    def test_reject_invalid_wire_data(self):
        for value in (b"", b"\xff", b"not json", b"[]", b" " * (receiver.MAX_DATAGRAM + 1),
                      b'{"schema":1,"schema":1}', b'{"x":NaN}',
                      b'{"x":Infinity}', b'{"x":1e999}'):
            with self.subTest(value=value[:50]), self.assertRaises(receiver.PacketError):
                receiver.parse_datagram(value)

    def test_reject_schema_and_field_type_errors(self):
        for patch in ({"schema":True},{"schema":2},{"type":"unknown"},
                      {"receiver_id":""},{"sequence":True},{"uptime_ms":-1},
                      {"additional":chr(0xd800)},{"ftm":{"distance_m":-1}},
                      {"ftm":{"rtt_ns":True}},{"wifi":[]}):
            value=packet();value.update(patch)
            with self.subTest(patch=patch),self.assertRaises(receiver.PacketError):
                receiver.parse_datagram(encode(value))

    def test_jsonl_keeps_receive_and_sample_times_separate(self):
        value = record()
        line = receiver.json_line(value)
        parsed = json.loads(line)
        self.assertEqual(parsed["source_ip"], "192.0.2.1")
        self.assertEqual(parsed["telemetry"]["ftm"]["age_ms"], 2000)
        self.assertTrue(parsed["received_at"].endswith("+00:00"))
        self.assertNotIn("\n", line)


class LoggingTests(unittest.TestCase):
    def test_rotation_is_bounded_and_remains_jsonl(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "radio.jsonl"
            logger = receiver.make_log(str(path), 1800, 2)
            for index in range(30):
                value = record()
                value["telemetry"]["sequence"] = index
                logger.info(receiver.json_line(value))
            for handler in logger.handlers:
                handler.close()
            files = list(Path(directory).glob("radio.jsonl*"))
            self.assertEqual(len(files), 3)
            for file in files:
                self.assertLessEqual(file.stat().st_size, 1800)
                for line in file.read_text().splitlines():
                    self.assertEqual(json.loads(line)["telemetry"]["schema"], 1)


class SocketTests(unittest.TestCase):
    @unittest.skipIf(sys.platform == "win32", "POSIX pipe readiness used by this integration test")
    def test_udp_input_to_stdout_and_graceful_interrupt(self):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
            probe.bind(("127.0.0.1", 0))
            port = probe.getsockname()[1]
        process = subprocess.Popen([sys.executable, "-u", str(ROOT / "tools" / "telemetry_receiver.py"),
                                    "--bind", "127.0.0.1", "--port", str(port)],
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            self.assertTrue(select.select([process.stderr], [], [], 5)[0], "listener did not start")
            self.assertIn("Listening", process.stderr.readline())
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
                sender.sendto(b'{"invalid":true}', ("127.0.0.1", port))
                sender.sendto(encode(packet()), ("127.0.0.1", port))
            self.assertTrue(select.select([process.stdout], [], [], 5)[0], "valid packet not received")
            result = json.loads(process.stdout.readline())
            self.assertEqual(result["source_ip"], "127.0.0.1")
            self.assertEqual(result["telemetry"]["ftm"]["distance_m"], 1.52)
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
                sender.sendto(encode(ahrs_packet()), ("127.0.0.1", port))
            self.assertTrue(select.select([process.stdout], [], [], 5)[0], "AHRS packet not received")
            ahrs_result = json.loads(process.stdout.readline())
            self.assertEqual(ahrs_result["telemetry"]["type"], "ahrs")
            self.assertEqual(ahrs_result["telemetry"]["ahrs"]["imu"]["accel_m_s2"][2], 9.80665)
            process.send_signal(signal.SIGINT)
            process.communicate(timeout=5)
            self.assertEqual(process.returncode, 0)
        finally:
            if process.poll() is None:
                process.kill()
                process.communicate()


class SummaryOrderingTests(unittest.TestCase):
    @staticmethod
    def summary(sequence=1, boot_id="boot-a", status="success"):
        return {"schema": 1, "type": "summary", "receiver_id": "cam-one",
                "boot_id": boot_id, "uptime_ms": 30000, "sequence": sequence,
                "ftm": {"status": status, "distance_m": 3.42, "rtt_ns": 23,
                        "age_ms": 123, "samples": 16}}

    def test_ftm_uint32_centimeter_boundary(self):
        value = self.summary()
        value["ftm"]["distance_m"] = 42949672.95
        parsed = receiver.parse_datagram(encode(value))
        self.assertEqual(parsed["ftm"]["distance_m"], 42949672.95)
        value["ftm"]["distance_m"] = 42949672.96
        with self.assertRaises(receiver.PacketError):
            receiver.parse_datagram(encode(value))

    def test_summary_reordering_does_not_restore_stale_ftm_status(self):
        cache = receiver.LatestObservations()
        cache.add(record(self.summary(10, status="success")), 1)
        cache.add(record(self.summary(9, status="failed")), 2)
        cache.add(record(self.summary(10, status="failed")), 3)
        value, arrived = next(iter(cache.summaries.values()))
        self.assertEqual(value["telemetry"]["ftm"]["status"], "success")
        self.assertEqual(arrived, 1)
        cache.add(record(self.summary(1, "boot-b", "measuring")), 4)
        value, arrived = next(iter(cache.summaries.values()))
        self.assertEqual(value["telemetry"]["boot_id"], "boot-b")
        self.assertEqual(value["telemetry"]["ftm"]["status"], "measuring")
        self.assertEqual(arrived, 4)

    def test_uint32_sequence_wrap_for_ftm_summaries(self):
        cache=receiver.LatestObservations()
        for sequence,now in ((0xfffffffe,1),(1,2),(0xffffffff,3)):
            cache.add(record(self.summary(sequence)),now)
        summary,arrived=next(iter(cache.summaries.values()))
        self.assertEqual(summary["telemetry"]["sequence"],1)
        self.assertEqual(arrived,2)


def ahrs_packet(sequence=1, boot_id="boot-a"):
    return {
        "schema": 1, "type": "ahrs", "receiver_id": "cam-one", "boot_id": boot_id,
        "uptime_ms": 30000, "timestamp_ms": None, "sequence": sequence,
        "ahrs": {
            "worker_ready": True, "paused": False,
            "imu": {"present": True, "valid": True, "age_ms": 10, "sample_count": 100,
                    "accel_m_s2": [0, 0, 9.80665], "gyro_rad_s": [0.01, 0.02, 0.03],
                    "temp_c": 25.0},
            "mag": {"present": True, "valid": True, "age_ms": 20,
                    "field_uT": [10.0, 12.0, -35.0],
                    "calibration": "factory_sensitivity_only"},
            "barometer": {"present": True, "valid": True, "age_ms": 30,
                          "pressure_pa": 101325.0, "temp_c": 23.5,
                          "pressure_altitude_m": 0.0},
            "attitude": {"valid": True, "age_ms": 10, "quaternion_wxyz": [1, 0, 0, 0],
                         "roll_deg": 0, "pitch_deg": 0, "yaw_deg": 0,
                         "reference": "relative_yaw", "generation": 1,
                         "yaw_origin_uptime_ms": 1000},
            "calibration": {"status": "uncalibrated", "gyro_bias_rad_s": [0, 0, 0],
                            "gyro_bias_valid": False, "samples": 0}
        }
    }


class AhrsTests(unittest.TestCase):
    def test_ahrs_jsonl_retains_units_timing_and_additive_metadata(self):
        value = ahrs_packet()
        value["ahrs"]["mounting"] = "sensor_axes"
        parsed = receiver.parse_datagram(encode(value))
        self.assertEqual(parsed, value)
        saved = json.loads(receiver.json_line(record(parsed)))
        self.assertEqual(saved["telemetry"]["ahrs"]["imu"]["age_ms"], 10)
        self.assertEqual(saved["telemetry"]["ahrs"]["attitude"]["reference"], "relative_yaw")

    def test_missing_sensors_use_null_measurements(self):
        value = ahrs_packet()
        data = value["ahrs"]
        for name, fields in (("imu", ("accel_m_s2", "gyro_rad_s", "temp_c")),
                             ("mag", ("field_uT",)),
                             ("barometer", ("pressure_pa", "temp_c", "pressure_altitude_m")),
                             ("attitude", ("quaternion_wxyz", "roll_deg", "pitch_deg", "yaw_deg"))):
            data[name]["valid"] = False
            data[name]["age_ms"] = None
            if name != "attitude":
                data[name]["present"] = False
            for field in fields:
                data[name][field] = None
        data["attitude"]["yaw_origin_uptime_ms"] = None
        self.assertEqual(receiver.parse_datagram(encode(value)), value)

    def test_reject_inconsistent_or_malformed_ahrs(self):
        mutations = [
            lambda x: x["ahrs"].update(paused=True),
            lambda x: x["ahrs"].update(worker_ready=False),
            lambda x: x["ahrs"]["imu"].update(present=False),
            lambda x: x["ahrs"]["imu"].update(valid=False),
            lambda x: x["ahrs"]["imu"].update(age_ms=None),
            lambda x: x["ahrs"]["imu"].update(gyro_rad_s=[0, 0]),
            lambda x: x["ahrs"]["mag"].update(field_uT=[0, True, 1]),
            lambda x: x["ahrs"]["barometer"].update(pressure_pa=float("nan")),
            lambda x: x["ahrs"]["attitude"].update(quaternion_wxyz=[2, 0, 0, 0]),
            lambda x: x["ahrs"]["attitude"].update(reference="true_north"),
            lambda x: x["ahrs"]["attitude"].update(yaw_origin_uptime_ms=None),
            lambda x: x["ahrs"]["calibration"].update(gyro_bias_valid=1),
        ]
        for mutation in mutations:
            value = ahrs_packet()
            mutation(value)
            with self.subTest(value=value), self.assertRaises(receiver.PacketError):
                receiver.parse_datagram(encode(value))

    def test_ahrs_cache_orders_packets_preserves_sources_and_expiry(self):
        cache = receiver.LatestObservations(maximum=2, ttl=5)
        for sequence, now in ((0xfffffffe, 1), (1, 2), (0xffffffff, 3)):
            cache.add(record(ahrs_packet(sequence)), now)
        value, arrived = next(iter(cache.ahrs.values()))
        self.assertEqual(value["telemetry"]["sequence"], 1)
        self.assertEqual(arrived, 2)
        cache.add(record(ahrs_packet(1, "boot-b")), 4)
        cache.add(record(ahrs_packet(), ip="192.0.2.2"), 4)
        self.assertEqual(len(cache.ahrs), 2)
        cache.add(record(ahrs_packet(), ip="192.0.2.3"), 4)
        self.assertEqual(len(cache.ahrs), 2)
        cache.rows(10)
        self.assertEqual(len(cache.ahrs), 0)

    def test_view_ages_ahrs_independently_and_does_not_show_stale_angles(self):
        cache = receiver.LatestObservations()
        cache.add(record(ahrs_packet()), 10)
        fresh = receiver.render_view(cache, 10.1, 0)
        self.assertIn("relative yaw", fresh)
        self.assertIn("1013.2 hPa", fresh)
        stale = receiver.render_view(cache, 11, 0)
        self.assertIn("attitude unavailable/stale", stale)
        self.assertNotIn("roll 0.0", stale)
        self.assertIn("pressure unavailable/stale", receiver.render_view(cache, 13, 0))


if __name__ == "__main__":
    unittest.main()
