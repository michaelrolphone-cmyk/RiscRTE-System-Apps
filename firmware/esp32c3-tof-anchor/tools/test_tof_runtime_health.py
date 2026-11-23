"""Keep real health HTTP responsive to slow and failed host clock exchange."""
import json
import threading
import time
import unittest
from types import SimpleNamespace
from urllib.error import HTTPError
from urllib.request import urlopen

from biomerte.host_io_client import ClockBounds
from tof_host_io import RuntimeHealth, native_output_clock


class RuntimeHealthClockTests(unittest.TestCase):
    snapshot = {"rover_measurements": {"age_s": 0},
                "anchors": [{"received_age_s": 0}], "camera_error": None}
    nav = SimpleNamespace(feed_error=None)

    def test_http_health_remains_responsive_during_clock_exchange(self):
        entered, release, received = threading.Event(), threading.Event(), threading.Event()
        errors, responses = [], []
        health = RuntimeHealth(0)
        health.note(self.snapshot, self.nav)

        class SlowHost:
            def clock(self):
                entered.set()
                if not release.wait(5):
                    raise OSError("test clock exchange was not released")
                return SimpleNamespace(valid=True)

        def update():
            try:
                health.note(self.snapshot, self.nav, SlowHost())
            except Exception as exc:
                errors.append(exc)

        def request():
            try:
                try:
                    response = urlopen("http://127.0.0.1:" +
                        str(health.server.server_port) + "/healthz", timeout=4)
                except HTTPError as exc:
                    response = exc
                with response:
                    responses.append((response.status, json.load(response)))
            except Exception as exc:
                errors.append(exc)
            finally:
                received.set()

        updater = threading.Thread(target=update)
        reader = threading.Thread(target=request)
        try:
            updater.start()
            self.assertTrue(entered.wait(2), "clock exchange did not start")
            reader.start()
            responsive = received.wait(1)
        finally:
            release.set()
            updater.join(5)
            if reader.ident is not None:
                reader.join(5)
            health.close()
        self.assertTrue(responsive, "health HTTP waited for the host clock exchange")
        self.assertFalse(updater.is_alive())
        self.assertFalse(reader.is_alive())
        self.assertEqual(errors, [])
        self.assertIn(responses[0][0], (200, 503))
        self.assertIn("live", responses[0][1])

    def test_failed_clock_is_unready_without_exiting_or_publishing_bad_time(self):
        now = time.monotonic()
        # The real clock policy rejects these bounds; retain its default limit.
        bounds = ClockBounds("host-boot", 0., .5, now, clock=lambda: now)

        class UnqualifiedHost:
            def clock(self):
                bounds._check()
                return bounds

        class UnavailableHost:
            def clock(self):
                raise OSError("host I/O unavailable")

        for host, error in ((UnqualifiedHost(), ValueError), (UnavailableHost(), OSError)):
            with self.subTest(host=type(host).__name__):
                health = RuntimeHealth(None)
                health.note(self.snapshot, self.nav)
                self.assertTrue(health.snapshot()["ready"])
                health.note(self.snapshot, self.nav, host)
                state = health.snapshot()
                self.assertTrue(state["live"])
                self.assertFalse(state["ready"])
                self.assertFalse(state["clock_qualified"])
                with self.assertRaises(error):
                    native_output_clock({"rover_ranges": [{"observed": 1.}]}, host)
                health.note(self.snapshot, self.nav)
                self.assertTrue(health.snapshot()["ready"])


if __name__ == "__main__":
    unittest.main()
