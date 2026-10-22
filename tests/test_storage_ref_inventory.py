import hashlib
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location("audit_storage_refs", Path(__file__).resolve().parents[1] / "scripts/audit_storage_refs.py")
audit = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(audit)


class StorageRefInventoryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.git("init", "-q")
        self.git("config", "user.email", "test@example.invalid")
        self.git("config", "user.name", "Inventory test")

    def git(self, *args):
        return subprocess.check_output(["git", "-C", str(self.root), *args]).decode().strip()

    def write(self, path, data):
        p = self.root / path
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data.encode() if isinstance(data, str) else data)

    def commit(self):
        self.git("add", ".")
        self.git("commit", "-qm", "fixture")
        return self.git("rev-parse", "HEAD")

    def scan(self, sha):
        return audit.scan_repository({"repository": "example/project", "commit": sha}, self.root)

    def test_immutable_bytes_ignore_dirty_worktree_index_and_later_head(self):
        original = '#include "RiscStorageFilesV1.h"\nSD.open("/sd/data");\n'
        self.write("Apps/client.c", original)
        first = self.commit()
        self.write("Apps/client.c", "no storage\n")
        self.write("Apps/later.c", "SD.open(\"later\");\n")
        self.commit()
        self.write("Apps/client.c", "SPIFFS\n")
        self.git("add", ".")
        report = self.scan(first)
        self.assertEqual([x["path"] for x in report["files"]], ["Apps/client.c"])
        item = report["files"][0]
        self.assertEqual(item["sha256"], hashlib.sha256(original.encode()).hexdigest())
        self.assertEqual(item["blob"], self.git("rev-parse", first + ":Apps/client.c"))
        self.assertEqual(item["hits"][1]["line"], 2)
        self.assertIn(first, item["url"])

    def test_arduino_recipes_and_configuration_are_covered(self):
        for path in ["src/main.ino", "scripts/build.py", ".github/workflows/build.yml", "platformio.ini", "CMakeLists.txt"]:
            self.write(path, "storage.installed-files\n")
        result = self.scan(self.commit())
        self.assertEqual(result["matching_files"], 5)
        self.assertEqual(result["scanned_by_role"]["configuration"], 3)

    def test_role_hints_keep_nonconsumers_distinct(self):
        cases = {"Apps/a.c": "application", "Services/files/a.c": "provider_or_service", "Drivers/sd/a.c": "provider_or_service", "sdk/include/api.h": "interface_or_header", "tests/a.c": "test_fixture", "archive/a.c": "historical", "assets/a.h": "generated_or_asset", "docs/evidence/a.json": "documentation_evidence", "vendor/a.c": "third_party"}
        for path, role in cases.items():
            self.assertEqual(audit.role_for(path), role)

    def test_symlinks_gitlinks_binary_nonutf8_and_oversize_are_explicit(self):
        self.write("src/data.c", b"SD\0binary")
        self.write("src/latin.c", b"SD\xff")
        self.write("src/large.c", b"a" * (audit.MAX_BLOB_BYTES + 1))
        self.write("src/target.c", "storage.files\n")
        (self.root / "src/link.c").symlink_to("target.c")
        first = self.commit()
        self.git("update-index", "--add", "--cacheinfo", "160000," + first + ",submodule")
        self.git("commit", "-qm", "gitlink")
        report = self.scan(self.git("rev-parse", "HEAD"))
        for reason in ["binary_blob", "non_utf8_blob", "oversize_blob", "symlink_not_followed", "gitlink_not_descended"]:
            self.assertEqual(report["skipped_by_reason"][reason], 1)
        self.assertEqual(report["matching_files"], 1)

    def test_nonimmutable_ref_is_rejected(self):
        with self.assertRaises(ValueError):
            self.scan("main")

    def test_absent_repository_fails_instead_of_success_with_zero_hits(self):
        with self.assertRaises(subprocess.CalledProcessError):
            audit.scan_repository({"repository": "example/missing", "commit": "0" * 40}, self.root / "missing")

    def test_empty_repository_and_duplicate_manifest_are_distinct(self):
        source = {"repository": "example/empty", "status": "empty_no_refs", "evidence": "https://api.github.com/repos/example/empty/branches"}
        report = audit.make_report({"repositories": [source]}, self.root)
        self.assertEqual(report["repositories"][0]["status"], "empty_no_refs")
        with self.assertRaises(ValueError):
            audit.make_report({"repositories": [source, source]}, self.root)


if __name__ == "__main__":
    unittest.main()
