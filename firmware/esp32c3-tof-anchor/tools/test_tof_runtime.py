import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from tof_runtime import OwnerLock, RoverRequest


class RuntimeCustodyTests(unittest.TestCase):
    def test_pending_survives_changed_monotonic_epoch_until_device_completion(self):
        with tempfile.TemporaryDirectory() as name:
            path = Path(name)/"request.json"
            request = RoverRequest(path, clock=lambda:1000., wall=lambda:2000.)
            request.observe(dict(boot_id="camera-boot",uptime_ms=5000,ftm={}))
            request.submitted("AA:00:00:00:00:01",998.)
            other = RoverRequest(path, clock=lambda:10., wall=lambda:2003.)
            self.assertEqual(other.pending,"aa:00:00:00:00:01")
            self.assertEqual(other.started,5.)
            other.observe(dict(boot_id="camera-boot",uptime_ms=8000,
                               ftm=dict(target="different",busy=True)))
            self.assertEqual(other.pending,"aa:00:00:00:00:01")
            other.observe(dict(boot_id="camera-boot",uptime_ms=9000,
                               ftm=dict(target=other.pending,status="success")))
            self.assertIsNone(RoverRequest(path).pending)

    def test_expired_mismatched_request_requires_fresh_definite_idle_report(self):
        with tempfile.TemporaryDirectory() as name:
            path = Path(name) / "request.json"
            request = RoverRequest(path, clock=lambda: 100., wall=lambda: 200.)
            idle = dict(target="previous", status="success", busy=False, retiring=False)
            request.observe(dict(boot_id="camera-boot"))
            request.submitted("different", 80.)
            for boot, observed, report in (
                ("camera-boot", 94., idle),
                ("camera-boot", 96., idle),
                ("camera-boot", 99., dict(idle, busy=True)),
                ("camera-boot", 99., dict(idle, retiring=True)),
                ("camera-boot", 99., dict(idle, status="measuring")),
                (None, 99., idle),
            ):
                request.observe(dict(boot_id=boot, ftm=report), observed)
                self.assertEqual(request.pending, "different")
            self.assertFalse(request.observe(dict(boot_id="camera-boot", ftm=idle), 99.))
            self.assertIsNone(RoverRequest(path).pending)

    def test_legacy_native_pending_state_is_retained_at_handoff(self):
        with tempfile.TemporaryDirectory() as name:
            root=Path(name);legacy=root/"latest.json"
            legacy.write_text(json.dumps(dict(camera_boot_id="native-boot",
                pending_target="aa:00:00:00:00:01",updated_at=1999.,
                request_age_s={"aa:00:00:00:00:01":3.})))
            state=RoverRequest(root/"custody.json",legacy,
                               clock=lambda:10.,wall=lambda:2000.)
            self.assertEqual(state.pending,"aa:00:00:00:00:01")
            self.assertEqual(state.started,6.)
            self.assertEqual(state.boot_id,"native-boot")

    def test_invalid_custody_does_not_silently_drop_uncertain_effect(self):
        with tempfile.TemporaryDirectory() as name:
            path=Path(name)/"custody.json"
            path.write_text('{"schema":"wrong"}')
            with self.assertRaises(ValueError):RoverRequest(path)

    def test_sensor_uptime_does_not_rewrite_custody_for_each_frame(self):
        with tempfile.TemporaryDirectory() as name:
            path=Path(name)/"custody.json";state=RoverRequest(path)
            state.observe(dict(boot_id="boot",uptime_ms=1))
            original=path.read_bytes();stamp=path.stat().st_mtime_ns
            for i in range(2,30):state.observe(dict(boot_id="boot",uptime_ms=i))
            self.assertEqual(original,path.read_bytes())
            self.assertEqual(stamp,path.stat().st_mtime_ns)

    def test_other_process_cannot_acquire_same_state_owner_until_release(self):
        with tempfile.TemporaryDirectory() as name:
            path=Path(name)/"owner.lock"
            program="from tof_runtime import OwnerLock; import sys; OwnerLock(sys.argv[1]).acquire()"
            with OwnerLock(path):
                denied=subprocess.run([sys.executable,"-c",program,str(path)],
                                      capture_output=True,text=True)
                self.assertNotEqual(denied.returncode,0)
                self.assertIn("BlockingIOError",denied.stderr)
            accepted=subprocess.run([sys.executable,"-c",program,str(path)],
                                    capture_output=True,text=True)
            self.assertEqual(accepted.returncode,0,accepted.stderr)

if __name__=="__main__":unittest.main()
