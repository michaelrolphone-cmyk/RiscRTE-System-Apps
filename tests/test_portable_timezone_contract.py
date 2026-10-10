import hashlib
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PortableTimezoneContract(unittest.TestCase):
    def test_frozen_catalog_and_watch_policy(self):
        record = json.loads((ROOT / 'lib/PortableApps/time/TIMEZONE_PROVENANCE.json').read_text())
        self.assertEqual(record['upstream_commit'], '34d8e694d89a1e72d8854403d8592c289fae3ddc')
        catalog = (ROOT / 'lib/PortableApps/src/PortableTimeZoneCatalog.c').read_bytes()
        self.assertEqual(hashlib.sha256(catalog).hexdigest(), record['portable_catalog_sha256'])
        self.assertEqual(len(re.findall(rb'\{"[^"]+", "[^"]+", PORTABLE_TIMEZONE_\w+\}', catalog)), 419)
        for path, digest in record['unchanged_watch_files'].items():
            self.assertEqual(hashlib.sha256((ROOT / path).read_bytes()).hexdigest(), digest, path)

    def test_no_process_or_libc_timezone_dependency(self):
        for path in (ROOT / 'lib/PortableApps/src').glob('PortableTimeZone*.c'):
            source = path.read_text()
            for symbol in ('getenv', 'setenv', 'putenv', 'tzset', 'mktime', 'localtime', 'gmtime', 'malloc', 'free'):
                self.assertNotRegex(source, rf'\b{symbol}(?:_r)?\s*\(', str(path))


if __name__ == '__main__':
    unittest.main()
