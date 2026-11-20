"""Behavior checks for untimed, streaming qualification commands."""
import contextlib
import importlib.util
import io
from pathlib import Path
import sys
import tempfile
import threading
import time
import unittest

spec = importlib.util.spec_from_file_location("tof_ci", Path(__file__).with_name("ci.py"))
ci = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ci)


class LoggedCommandTests(unittest.TestCase):
    def test_progress_is_saved_before_child_exits(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            log, acknowledgement = folder / "stage.log", folder / "ack"
            stop = threading.Event()

            def acknowledge_logged_progress():
                while not stop.wait(0.005):
                    if log.exists() and b"ready\n" in log.read_bytes():
                        acknowledgement.write_text("observed")
                        return

            observer = threading.Thread(target=acknowledge_logged_progress)
            observer.start()
            # This fixture's deadline detects a broken live-log handshake.
            # Production qualification subprocesses have no wall-clock cutoff.
            child = (
                "import pathlib,sys,time\n"
                "print('ready', flush=True)\n"
                "ack=pathlib.Path(sys.argv[1]); deadline=time.monotonic()+5\n"
                "while not ack.exists() and time.monotonic()<deadline: time.sleep(0.005)\n"
                "if not ack.exists(): sys.exit(23)\n"
                "print('finished', flush=True)\n"
            )
            try:
                with contextlib.redirect_stdout(io.StringIO()) as console:
                    code = ci.logged_command(
                        [sys.executable, "-u", "-c", child, str(acknowledgement)], log)
            finally:
                stop.set()
                observer.join()
            self.assertEqual(code, 0)
            self.assertEqual(log.read_text(), "ready\nfinished\n")
            self.assertEqual(console.getvalue(), log.read_text())

    def test_nonzero_exit_keeps_both_streams_and_last_fragment(self):
        with tempfile.TemporaryDirectory() as temporary:
            log = Path(temporary) / "failed.log"
            child = "import os,sys;os.write(1,b'out\\n');os.write(2,b'error without newline');sys.exit(9)"
            with contextlib.redirect_stdout(io.StringIO()) as console:
                code = ci.logged_command([sys.executable, "-u", "-c", child], log)
            self.assertEqual(code, 9)
            self.assertEqual(log.read_bytes(), b"out\nerror without newline")
            self.assertEqual(console.getvalue(), log.read_text())


if __name__ == "__main__":
    unittest.main()
