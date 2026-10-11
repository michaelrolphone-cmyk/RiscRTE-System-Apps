import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from build_portable_springboard import catalog_source


class SpringboardCatalogBound(unittest.TestCase):
    def catalog(self, directory, count):
        path = Path(directory) / "catalog.json"
        path.write_text(json.dumps({"apps": [
            {"display_name": "App " + str(i), "file_name": "app" + str(i) + ".elf", "icon": "solid:f017"}
            for i in range(count)
        ]}))
        return path

    def test_selected_page_boundaries(self):
        with tempfile.TemporaryDirectory() as directory:
            for count in (20, 21, 22, 40):
                source, record = catalog_source(self.catalog(directory, count), 40)
                self.assertEqual(record["count"], count)
                self.assertIn("portable_catalog_count=" + str(count), source)
            with self.assertRaisesRegex(ValueError, "at most 40"):
                catalog_source(self.catalog(directory, 41), 40)

    def test_other_profiles_keep_existing_bound(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertEqual(catalog_source(self.catalog(directory, 18))[1]["count"], 18)
            with self.assertRaisesRegex(ValueError, "at most 18"):
                catalog_source(self.catalog(directory, 19))

    def test_gamepad_is_only_admitted_for_selected_profile(self):
        with tempfile.TemporaryDirectory() as directory:
            path = self.catalog(directory, 1)
            data = json.loads(path.read_text())
            data["apps"][0]["icon"] = "solid:f11b"
            path.write_text(json.dumps(data))
            self.assertEqual(catalog_source(path, 40)[1]["count"], 1)
            with self.assertRaisesRegex(ValueError, "absent from licensed subset"):
                catalog_source(path)

    def test_compiled_bound_rejects_out_of_range(self):
        header = ROOT / "lib/PortableApps/include/PortableSpringboardCatalog.h"
        for count in (0, 41):
            result = subprocess.run(["cc", "-x", "c", "-fsyntax-only", "-include", str(header),
                "-DPORTABLE_SPRINGBOARD_CATALOG_BOUND=" + str(count), "/dev/null"], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"explicit bounded build contract", result.stderr)


if __name__ == "__main__":
    unittest.main()
