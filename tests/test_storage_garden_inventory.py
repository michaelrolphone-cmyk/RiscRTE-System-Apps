"""Integrity and scope checks for the additive, pinned Garden source inventory."""
import hashlib
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
REPORT = ROOT / "docs/storage/inventory-20261023-garden"
PIN = "2b01d37ac5a6b643b8bc25758a07787587d7ca7e"
REPO = "michaelrolphone-cmyk/Garden-Controller"


class GardenInventoryTests(unittest.TestCase):
    def setUp(self):
        self.manifest = json.loads((REPORT / "sources.json").read_text())
        self.index = json.loads((REPORT / "evidence/index.json").read_text())
        self.row = self.index["repositories"][0]
        self.raw = (REPORT / "evidence" / self.row["report_file"]).read_bytes()
        self.report = json.loads(self.raw)

    def test_manifest_and_report_are_pinned_and_hash_bound(self):
        self.assertEqual(len(self.manifest["repositories"]), 1)
        source = self.manifest["repositories"][0]
        self.assertEqual((source["repository"], source["commit"]), (REPO, PIN))
        self.assertEqual(self.report["commit"], PIN)
        canonical = json.dumps(self.manifest, sort_keys=True, separators=(",", ":")).encode()
        self.assertEqual(hashlib.sha256(canonical).hexdigest(), self.index["source_manifest_sha256"])
        self.assertEqual(hashlib.sha256(self.raw).hexdigest(), self.row["report_sha256"])

    def test_coverage_counts_and_locations_are_consistent(self):
        report = self.report
        self.assertEqual(report["scanned_files"], sum(report["scanned_by_role"].values()))
        self.assertEqual(report["tracked_entries"], report["scanned_files"] + sum(report["skipped_by_reason"].values()))
        self.assertEqual(report["matching_files"], len(report["files"]))
        self.assertEqual(len(report["files"]), len({f["path"] for f in report["files"]}))
        for item in report["files"]:
            self.assertTrue(item["url"].startswith(f"https://github.com/{REPO}/blob/{PIN}/"))
            self.assertRegex(item["blob"], r"^[0-9a-f]{40}$")
            self.assertRegex(item["sha256"], r"^[0-9a-f]{64}$")
            self.assertTrue(all(h["line"] > 0 and h["kinds"] for h in item["hits"]))
            self.assertTrue(all(set(h) == {"line", "kinds"} for h in item["hits"]))

    def test_host_tooling_is_not_reported_as_application_migration(self):
        files = {f["path"]: f for f in self.report["files"]}
        for path in ["scripts/build_relay_deployment.py", "scripts/build_relay_store.py", "scripts/verify_relay_deployment.py"]:
            self.assertEqual(files[path]["role_hint"], "tooling")
        self.assertEqual(files["riscrte/Drivers/storage/driver.c"]["role_hint"], "provider_or_service")
        self.assertNotIn("riscrte/Apps/garden-relay.c", files)
        self.assertTrue(self.index["limitations"])

    def test_workflow_reproduces_pinned_bytes_and_retains_original_coverage(self):
        workflow = (ROOT / ".github/workflows/storage-inventory.yml").read_text()
        self.assertIn("ref: " + PIN, workflow)
        self.assertIn("diff -ru docs/storage/inventory-20261023-garden/evidence", workflow)
        self.assertIn("docs/storage/inventory-20261022/**", workflow)
        self.assertIn("test_storage_garden_inventory.py", workflow)
        historical = json.loads((ROOT / "docs/storage/inventory-20261022/sources.json").read_text())
        garden = next(s for s in historical["repositories"] if s["repository"] == REPO)
        self.assertEqual(garden["commit"], "2b27fec49c2bb33914f53821f2a530578d5578cc")


if __name__ == "__main__":
    unittest.main()
