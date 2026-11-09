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


def packet(receiver_id="cam-one", radio="ble", address_type=1):
    return {"schema": 1, "type": "observation", "receiver_id": receiver_id,
            "boot_id": "boot-a", "uptime_ms": 30000, "timestamp_ms": None,
            "sequence": 1, "radio": radio,
            "observation": {"id": "aa:bb:cc:dd:ee:ff/1", "address": "aa:bb:cc:dd:ee:ff",
                            "address_type": address_type, "name": "Beacon",
                            "rssi_dbm": -64, "rssi_filtered_dbm": -63.5,
                            "sample_count": 4, "age_ms": 2000,
                            "distance_m": 1.52, "distance_method": "rssi_assumed",
                            "reference_rssi_1m_dbm": -59,
                            "path_loss_exponent": 2.5, "calibrated": False}}


def encode(value):
    return json.dumps(value).encode("utf-8")


def record(value=None, ip="192.0.2.1"):
    return receiver.received_record(value if value is not None else packet(), (ip, 9876))


class ParsingTests(unittest.TestCase):
    def test_observation_preserves_additive_metadata(self):
        value = packet()
        value["additional"] = {"clock": "unsynchronized"}
        self.assertEqual(receiver.parse_datagram(encode(value)), value)

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
        mutations = [
            lambda x: x.update(schema=True),
            lambda x: x.update(schema=2),
            lambda x: x.update(type="not-observation"),
            lambda x: x.update(receiver_id=""),
            lambda x: x.update(sequence=True),
            lambda x: x.update(uptime_ms=-1),
            lambda x: x.update(radio="bluetooth-classic"),
            lambda x: x["observation"].update(address="wrong"),
            lambda x: x["observation"].update(rssi_dbm="strong"),
            lambda x: x["observation"].update(rssi_dbm=10**1000),
            lambda x: x.update(additional="\ud800"),
            lambda x: x["observation"].update(age_ms=-1),
            lambda x: x["observation"].update(sample_count=0),
            lambda x: x["observation"].update(distance_m=-5),
            lambda x: x["observation"].update(distance_method="precise-rssi"),
            lambda x: x["observation"].update(calibrated=1),
            lambda x: x["observation"].pop("address_type"),
        ]
        for mutation in mutations:
            value = packet()
            mutation(value)
            with self.subTest(value=value), self.assertRaises(receiver.PacketError):
                receiver.parse_datagram(encode(value))

    def test_jsonl_keeps_receive_and_sample_times_separate(self):
        value = record()
        line = receiver.json_line(value)
        parsed = json.loads(line)
        self.assertEqual(parsed["source_ip"], "192.0.2.1")
        self.assertEqual(parsed["telemetry"]["observation"]["age_ms"], 2000)
        self.assertTrue(parsed["received_at"].endswith("+00:00"))
        self.assertNotIn("\n", line)


class CacheTests(unittest.TestCase):
    def test_receivers_peers_and_ble_address_types_do_not_merge(self):
        cache = receiver.LatestObservations()
        for value in (record(), record(packet("cam-two")), record(ip="192.0.2.2"),
                      record(packet(address_type=0))):
            cache.add(value, 10)
        self.assertEqual(len(cache.rows(11)), 4)

    def test_age_increments_and_expired_observations_are_removed(self):
        cache = receiver.LatestObservations(ttl=5)
        cache.add(record(), 10)
        self.assertEqual(cache.rows(12)[0][1], 4)
        self.assertEqual(cache.rows(14), [])
        self.assertEqual(len(cache.items), 0)

    def test_repeated_stale_packets_do_not_restore_freshness(self):
        cache = receiver.LatestObservations(ttl=5)
        value = packet()
        value["observation"]["age_ms"] = 6000
        cache.add(record(value), 100)
        self.assertEqual(cache.rows(100), [])

    def test_capacity_evicts_oldest(self):
        cache = receiver.LatestObservations(maximum=2)
        for name in ("one", "two", "three"):
            cache.add(record(packet(name)), 1)
        self.assertEqual([x[0]["telemetry"]["receiver_id"] for x in cache.rows(1)],
                         ["two", "three"])

    def test_same_boot_out_of_order_ignored_new_boot_accepted(self):
        cache = receiver.LatestObservations()
        latest = packet()
        latest["sequence"] = 5
        cache.add(record(latest), 10)
        old = packet()
        cache.add(record(old), 11)
        self.assertEqual(cache.rows(12)[0][0]["telemetry"]["sequence"], 5)
        reboot = packet()
        reboot["boot_id"] = "boot-b"
        cache.add(record(reboot), 13)
        self.assertEqual(cache.rows(14)[0][0]["telemetry"]["boot_id"], "boot-b")

    def test_view_sanitizes_broadcast_names(self):
        value = packet()
        value["observation"]["name"] = "\x1b[2J\u202edanger"
        cache = receiver.LatestObservations()
        cache.add(record(value), 1)
        view = receiver.render_view(cache, 1, 0)
        self.assertNotIn("\x1b", view)
        self.assertNotIn("\u202e", view)
        self.assertIn("rssi_assumed", view)


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
            self.assertEqual(result["telemetry"]["observation"]["rssi_dbm"], -64)
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

    def test_uint32_sequence_wrap_for_observations_and_summaries(self):
        cache = receiver.LatestObservations()
        for sequence, now in ((0xfffffffe, 1), (1, 2), (0xffffffff, 3)):
            value = packet()
            value["sequence"] = sequence
            cache.add(record(value), now)
            cache.add(record(self.summary(sequence)), now)
        observation, age = cache.rows(4)[0]
        self.assertEqual(observation["telemetry"]["sequence"], 1)
        self.assertEqual(age, 4)
        summary, arrived = next(iter(cache.summaries.values()))
        self.assertEqual(summary["telemetry"]["sequence"], 1)
        self.assertEqual(arrived, 2)


if __name__ == "__main__":
    unittest.main()
