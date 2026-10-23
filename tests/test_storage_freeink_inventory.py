"""Integrity and scope checks for the additive, pinned FreeInk source inventory."""
import hashlib
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
REPORT = ROOT / "docs/storage/inventory-20261023-freeink"
PIN = "425d200a8ea447326b4b9696e4e47dc84ad9d7f6"
REPO = "Free-Ink/freeink-sdk"


class FreeInkInventoryTests(unittest.TestCase):
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

    def test_nested_boundary_is_explicit_not_false_closure(self):
        self.assertEqual(self.report['tracked_entries'], 891)
        self.assertEqual(self.report['scanned_files'], 769)
        self.assertEqual(self.report['matching_files'], 83)
        self.assertEqual(self.report['unresolved_paths'], [{
            'path': 'libs/assets/Icons/lucide', 'reason': 'gitlink_not_descended',
            'commit': 'c81680e066f45b640743ca78ae36cdedda3f0318'}])
        files = {f['path']: f for f in self.report['files']}
        self.assertEqual(files['libs/book/FreeInkBook/include/BookStorage.h']['role_hint'], 'interface_or_header')
        self.assertIn('test_fixture', self.report['scanned_by_role'])
        self.assertTrue(self.index['limitations'])

    def test_workflow_verifies_chain_and_preserves_prior_coverage(self):
        provenance = json.loads((REPORT / 'provenance.json').read_text())
        edges = provenance['gitlink_edges']
        self.assertEqual(len(edges), 2)
        self.assertEqual(edges[0]['child_commit'], edges[1]['commit'])
        self.assertEqual(edges[1]['child_commit'], PIN)
        self.assertEqual(edges[1]['url'], 'https://github.com/Free-Ink/freeink-sdk.git')
        self.assertEqual(self.manifest['repositories'][0]['source_ref'],
                         edges[1]['repository'] + '@' + edges[1]['commit'] + ':' + edges[1]['path'])
        workflow = (ROOT / '.github/workflows/storage-inventory.yml').read_text()
        for pin in [PIN, edges[0]['commit'], edges[1]['commit']]:
            self.assertIn('ref: ' + pin, workflow)
        for folder in ['inventory-20261022', 'inventory-20261023-garden', 'inventory-20261023-freeink']:
            self.assertIn('docs/storage/' + folder + '/**', workflow)
        self.assertIn("git('ls-tree'", workflow)
        self.assertIn("git('config', '--blob'", workflow)
        self.assertIn('diff -ru docs/storage/inventory-20261023-freeink/evidence', workflow)
        self.assertIn('diff -ru docs/storage/inventory-20261023-garden/evidence', workflow)


if __name__ == '__main__':
    unittest.main()
