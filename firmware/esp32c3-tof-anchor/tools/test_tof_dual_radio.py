"""Rear initiation respects explicit firmware disable markers without dropping observations."""
import json
import tempfile
import threading
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock, patch

from tof_dual_radio import DualRadioRanging


class RearRangingEligibilityTests(unittest.TestCase):
    NOW = 100.0
    ANCHOR = "aa:00:00:00:00:01"

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        root = Path(self.tmp.name)
        identity = [[int(i == j) for j in range(4)] for i in range(4)]
        (root / "survey-world.json").write_text(json.dumps({
            "world_id": "test-world", "survey_revision": 1,
            "reference": {"T_enu_body": identity, "T_enu_camera": identity},
            "features": [{"kind": "ftm", "peer": self.ANCHOR, "vertices_m": [[1, 0, 0]]}],
        }))
        (root / "camera-rig.json").write_text(json.dumps({
            "revision": 1, "rear": {"camera": {"device_id": "rear-test"},
                                  "T_front_camera_rear_camera_m": identity},
        }))
        (root / "config.json").write_text(json.dumps({"camera": {"device_id": "front-test"}}))
        self.ranging = DualRadioRanging(root, SimpleNamespace(lock=threading.Lock(), survey_pose=None))
        self.graph = SimpleNamespace(nodes={
            self.ANCHOR: {"received": self.NOW, "packet": {"channel": 3, "ftm_state": "idle"}},
        })
        self.ranging.request = Mock(return_value=200)

    def tearDown(self):
        self.ranging.pool.shutdown(wait=True)
        self.tmp.cleanup()

    def receive(self, ftm, *, side="rear", receiver=None, now=None):
        now = self.NOW if now is None else now
        with patch("tof_dual_radio.time.monotonic", return_value=now):
            self.ranging.receive({
                "sensor_tick": True, "camera_id": side,
                "capture_host_monotonic_interval_s": [now - .01, now],
                "sensor_packet": {"receiver_id": receiver or side + "-test",
                                  "boot_id": side + "-boot", "ftm": ftm},
            })

    def finish_request(self, now):
        command = self.ranging.command
        self.assertIsNotNone(command)
        command.result(timeout=2)
        self.ranging.tick(self.graph, now)
        self.assertIsNone(self.ranging.command)

    def test_each_explicit_marker_blocks_new_rear_requests(self):
        for markers in (
            {"supported": False},
            {"enabled": False},
            {"status": "disabled_in_firmware"},
            {"supported": False, "enabled": False, "status": "disabled_in_firmware",
             "busy": False, "retiring": False, "report_id": 0},
        ):
            with self.subTest(markers=markers):
                self.receive(markers)
                self.ranging.tick(self.graph, self.NOW)
                self.ranging.request.assert_not_called()
                self.assertIsNone(self.ranging.command)
                self.assertEqual(self.ranging.requests, {})

    def test_supported_legacy_and_ordinary_failure_remain_eligible(self):
        for i, state in enumerate((
            {},
            {"supported": True, "enabled": True, "status": "never_requested"},
            {"supported": True, "enabled": True, "status": "failed"},
            {"supported": None, "enabled": None},
            {"supported": 0, "enabled": ""},
        )):
            with self.subTest(state=state):
                now = self.NOW + i * 3
                self.ranging.request.reset_mock()
                self.receive(state, now=now)
                self.ranging.tick(self.graph, now)
                self.finish_request(now)
                self.ranging.request.assert_called_once_with(self.ANCHOR, 3)

    def test_fresh_supported_state_can_resume_after_disabled_state(self):
        self.receive({"supported": False, "enabled": False, "status": "disabled_in_firmware"})
        self.ranging.tick(self.graph, self.NOW)
        self.ranging.request.assert_not_called()
        self.receive({"supported": True, "enabled": True, "status": "never_requested"})
        self.ranging.tick(self.graph, self.NOW)
        self.finish_request(self.NOW)
        self.ranging.request.assert_called_once_with(self.ANCHOR, 3)

    def test_disabled_rear_still_records_pending_front_and_rear_observations(self):
        report = {"status": "success", "target": self.ANCHOR, "age_ms": 0,
                  "distance_m": 2., "report_id": 1, "busy": False, "retiring": False}
        self.ranging.cal[self.ANCHOR] = {"front_bias_m": .1, "rear_bias_m": .2}
        calibration = dict(self.ranging.cal)
        self.receive(report, side="front")
        self.receive(report)
        self.receive({"supported": False, "enabled": False, "status": "disabled_in_firmware"})
        self.ranging.tick(self.graph, self.NOW)
        self.assertEqual(self.ranging.counts, {"front": 1, "rear": 1})
        self.assertEqual(set(self.ranging.latest), {("front", self.ANCHOR), ("rear", self.ANCHOR)})
        self.assertEqual(self.ranging.pending, {})
        self.assertEqual(self.ranging.cal, calibration)
        self.ranging.request.assert_not_called()

    def test_running_command_keeps_custody_then_is_reaped_while_disabled(self):
        started, release = threading.Event(), threading.Event()

        def request(target, channel):
            started.set()
            if not release.wait(2):
                raise AssertionError("test did not release the in-flight command")
            return 200

        self.ranging.request = Mock(side_effect=request)
        try:
            self.receive({})
            self.ranging.tick(self.graph, self.NOW)
            self.assertTrue(started.wait(2))
            command = self.ranging.command
            self.receive({"enabled": False})
            self.ranging.tick(self.graph, self.NOW)
            self.assertIs(self.ranging.command, command)
            self.assertFalse(command.cancelled())
            self.assertFalse(command.done())
            release.set()
            command.result(timeout=2)
            self.ranging.tick(self.graph, self.NOW)
            self.assertIsNone(self.ranging.command)
            self.assertIsNone(self.ranging.error)
            self.ranging.request.assert_called_once_with(self.ANCHOR, 3)
        finally:
            release.set()

    def test_completed_command_error_is_preserved_without_resubmitting(self):
        self.ranging.request.side_effect = OSError("prior rear request failed")
        self.receive({})
        self.ranging.tick(self.graph, self.NOW)
        with self.assertRaisesRegex(OSError, "prior rear request failed"):
            self.ranging.command.result(timeout=2)
        self.receive({"supported": False})
        self.ranging.tick(self.graph, self.NOW)
        self.assertIsNone(self.ranging.command)
        self.assertEqual(self.ranging.error, "prior rear request failed")
        self.ranging.request.assert_called_once_with(self.ANCHOR, 3)

    def test_busy_and_retiring_still_block_supported_and_legacy_sessions(self):
        for flag in ("busy", "retiring"):
            for markers in ({}, {"supported": True, "enabled": True}):
                with self.subTest(flag=flag, markers=markers):
                    self.receive(dict(markers, **{flag: True}))
                    self.ranging.tick(self.graph, self.NOW)
                    self.ranging.request.assert_not_called()

    def test_front_or_wrong_receiver_disable_does_not_gate_rear(self):
        self.receive({})
        self.receive({"enabled": False}, side="front")
        self.receive({"enabled": False}, receiver="unrelated-rear")
        self.ranging.tick(self.graph, self.NOW)
        self.finish_request(self.NOW)
        self.ranging.request.assert_called_once_with(self.ANCHOR, 3)


if __name__ == "__main__":
    unittest.main()
