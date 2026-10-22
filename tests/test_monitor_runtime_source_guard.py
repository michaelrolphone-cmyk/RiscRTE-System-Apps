"""Real Git negative controls for stacked and flattened monitor source custody."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location(
    "monitor_runtime_bridge", Path(__file__).resolve().parents[1] / "scripts/test_monitor_runtime_bridge.py")
BRIDGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BRIDGE)


class SourceGuardTest(unittest.TestCase):
    def git(self, *args):
        return subprocess.check_output(["git", "-C", str(self.root), *args], text=True).strip()

    def write(self, path, text):
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text)

    def commit(self):
        self.git("add", ".")
        self.git("commit", "-qm", "fixture")
        return self.git("rev-parse", "HEAD")

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.git("init", "-q")
        self.git("config", "user.name", "Source guard fixture")
        self.git("config", "user.email", "fixture@example.invalid")
        for path in ("Apps/serial_monitor.c", "lib/PortableApps/src/adapter.c",
                     "docs/evidence/original.json", "test/native_apps/serial_monitor_test.c"):
            self.write(path, "original\n")
        self.root_commit = self.commit()
        self.write("docs/SERIAL_MONITOR_QUALIFICATION.md", "stacked cohort\n")
        self.stacked = self.commit()
        self.git("checkout", "--detach", "-q", self.root_commit)
        self.write("lib/PortableApps/src/adapter.c", "current main plus monitor guards\n")
        self.flattened = self.commit()
        self.tree = self.git("rev-parse", "HEAD^{tree}")
        for name, value in (("ROOT", self.root), ("BASE", self.stacked),
                            ("CONSOLIDATED_SOURCE", self.flattened), ("CONSOLIDATED_TREE", self.tree)):
            patcher = patch.object(BRIDGE, name, value)
            patcher.start()
            self.addCleanup(patcher.stop)

    def test_flattened_exact_cohort_is_accepted(self):
        self.assertEqual(BRIDGE.verify_source_scope(self.flattened), self.flattened)

    def test_original_stacked_cohort_is_still_accepted(self):
        self.git("checkout", "--detach", "-q", self.stacked)
        self.write("test/native_apps/monitor_runtime/fixture.c", "bridge fixture\n")
        head = self.commit()
        self.assertEqual(BRIDGE.verify_source_scope(head), self.stacked)

    def test_only_isolated_qualification_changes_are_accepted(self):
        for path in ("scripts/test_monitor_runtime_bridge.py", ".github/workflows/monitor-runtime-bridge.yml",
                     "docs/MONITOR_RUNTIME_BRIDGE.md", "tests/test_monitor_runtime_source_guard.py",
                     "test/native_apps/monitor_runtime/fixture.c"):
            self.write(path, "qualification only\n")
        head = self.commit()
        self.assertEqual(BRIDGE.verify_source_scope(head), self.flattened)

    def test_production_and_original_tests_and_evidence_cannot_change(self):
        for path in ("Apps/serial_monitor.c", "lib/PortableApps/src/adapter.c",
                     "docs/evidence/original.json", "test/native_apps/serial_monitor_test.c"):
            with self.subTest(path=path):
                self.write(path, "changed\n")
                with self.assertRaises(AssertionError):
                    BRIDGE.verify_source_scope(self.flattened)
                self.git("restore", path)

    def test_committed_production_mutation_is_rejected(self):
        self.write("Apps/serial_monitor.c", "changed\n")
        head = self.commit()
        with self.assertRaises(AssertionError):
            BRIDGE.verify_source_scope(head)

    def test_unrecognized_ancestry_is_rejected(self):
        self.git("checkout", "--detach", "-q", self.root_commit)
        with self.assertRaises(AssertionError):
            BRIDGE.verify_source_scope(self.root_commit)

    def test_wrong_consolidated_tree_is_rejected(self):
        with patch.object(BRIDGE, "CONSOLIDATED_TREE", "0" * 40):
            with self.assertRaises(AssertionError):
                BRIDGE.verify_source_scope(self.flattened)


if __name__ == "__main__":
    unittest.main()
