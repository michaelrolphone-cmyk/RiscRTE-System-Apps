import copy
import json
import pathlib
import sys
import unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import telemetry_receiver as rx

def packet():
    return {"schema": 1, "type": "ultrasonic",
            "receiver_id": "esp32cam-2884854b5798", "boot_id": "ffffffffffffffff",
            "uptime_ms": 4294967295, "sequence": 4294967295,
            "timestamp_ms": 1794360000000,
            "ultrasonic": {"schema": "hcsr04.v1", "worker_ready": True,
                "paused": False, "trigger_interval_ms": 65, "stale_after_ms": 1000,
                "sound_speed_m_s": 340, "sensors": [
                    {"id": name, "sequence": 4294967295, "valid": True, "status": "ok",
                     "distance_m": 3.99993, "echo_us": 23529,
                     "observed_uptime_ms": 4294967295, "age_ms": 1000}
                    for name in ("front", "left", "right", "rear")]}}

class SonarTests(unittest.TestCase):
    def parse(self, data):
        return rx.parse_datagram(json.dumps(data, separators=(",", ":")).encode())
    def test_valid_and_bounded_wire_packet(self):
        data = packet()
        self.assertLess(len(json.dumps(data, separators=(",", ":")).encode()), 1200)
        self.assertEqual(self.parse(data), data)
    def test_faults_are_null(self):
        for status in ("timeout", "echo_stuck_high", "out_of_range", "not_sampled",
                       "paused", "worker_unavailable"):
            data = packet()
            for sensor in data["ultrasonic"]["sensors"]:
                sensor.update(valid=False, status=status, distance_m=None, echo_us=None)
            self.parse(data)
            self.assertLess(len(json.dumps(data, separators=(",", ":")).encode()), 1200)
    def test_reject_invalid_values(self):
        for patch in ({"valid": True, "distance_m": None}, {"age_ms": 1001},
                      {"echo_us": 30000}, {"distance_m": 0}, {"valid": False}):
            data = packet()
            data["ultrasonic"]["sensors"][0].update(patch)
            with self.assertRaises(rx.PacketError): self.parse(data)
        data = packet()
        data["ultrasonic"]["sensors"][1]["id"] = "front"
        with self.assertRaises(rx.PacketError): self.parse(data)
    def test_paused_and_worker_failure_reject_fresh(self):
        for patch in ({"paused": True}, {"worker_ready": False}):
            data = packet()
            data["ultrasonic"].update(patch)
            with self.assertRaises(rx.PacketError): self.parse(data)
    def test_cache_orders_packets_and_ages_readings(self):
        cache = rx.LatestObservations()
        data = packet()
        for sensor in data["ultrasonic"]["sensors"]: sensor["age_ms"] = 0
        record = rx.received_record(data, ("192.0.2.1", 9876))
        cache.add(record, 0)
        older = copy.deepcopy(record)
        older["telemetry"]["sequence"] -= 1
        cache.add(older, 1)
        self.assertEqual(next(iter(cache.ultrasonic.values()))[1], 0)
        self.assertIn("4.000 m", rx.render_view(cache, 0, 0))
        self.assertIn("stale", rx.render_view(cache, 2, 0))
        cache.rows(181)
        self.assertFalse(cache.ultrasonic)

if __name__ == "__main__": unittest.main()
