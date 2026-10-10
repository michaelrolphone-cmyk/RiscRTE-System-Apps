"""Retain the audited legacy table prefixes alongside the additive snapshot."""
import hashlib
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class AdditiveSdkCompatibility(unittest.TestCase):
    def test_previous_table_declarations_are_exact_prefixes(self):
        audit = json.loads((ROOT / 'sdk/additive-api-rebaseline-20261010.json').read_text())
        for header in audit['headers']:
            with self.subTest(header=header['path']):
                data = (ROOT / header['path']).read_bytes()
                self.assertEqual(hashlib.sha256(data).hexdigest(), header['current_sha256'])
                source = re.sub(r'/\*.*?\*/|//[^\n]*', '', data.decode(), flags=re.S)
                table = re.search(r'typedef\s+struct\s*\{([^{}]*)\}\s*' +
                                  header['table'] + r'\s*;', source, re.S)
                self.assertIsNotNone(table)
                declarations = re.sub(r'\s+', '', table[1])
                boundary = header['preserved_prefix_characters']
                prefix, suffix = declarations[:boundary], declarations[boundary:]
                self.assertEqual(hashlib.sha256(prefix.encode()).hexdigest(),
                                 header['preserved_prefix_sha256'])
                self.assertEqual(re.findall(r'\(\*(\w+)\)', suffix), header['appended_members'])
        for header in audit['additional_headers']:
            with self.subTest(header=header['path']):
                data = (ROOT / header['path']).read_bytes()
                self.assertEqual(hashlib.sha256(data).hexdigest(), header['current_sha256'])
                source = re.sub(r'/\*.*?\*/|//[^\n]*', '', data.decode(), flags=re.S)
                source = re.sub(r'\s+', '', source)
                start = source.index(header['added_section_start'])
                end = source.index(header['added_section_end'], start) if header['added_section_end'] else len(source)
                preserved = source[:start] + source[end:]
                self.assertEqual(hashlib.sha256(preserved.encode()).hexdigest(), header['preserved_source_sha256'])
        self.assertFalse(audit['exports_changed'])
        self.assertEqual(hashlib.sha256((ROOT / 'sdk/firmware-exports.json').read_bytes()).hexdigest(),
                         audit['exports_sha256'])


if __name__ == '__main__':
    unittest.main()
