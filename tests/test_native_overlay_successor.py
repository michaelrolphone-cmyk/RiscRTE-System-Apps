"""Frozen released controls and exact native-overlay recipe comparison gates."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import report_native_overlay_successor as reporter

class OverlaySuccessor(unittest.TestCase):
    def fixture(self,app,root):
        baseline=ROOT/'docs/native-overlays'/('released-'+app)
        record=json.loads((baseline/(app+'-build-record.json')).read_text())
        memory_name='memory-admission.json' if app=='settings' else 'elf-memory-admission.json'
        memory=json.loads((baseline/memory_name).read_text())
        command=json.loads((baseline/'command.json').read_text())
        prefix='-DPORTABLE_'+('SETTINGS' if app=='settings' else 'WIFI')+'_VERSION='
        version='1.4.2' if app=='settings' else '1.4.1'
        command=[prefix+'"'+version+'"' if v.startswith(prefix) else v for v in command]
        command+=sorted(reporter.SELECTORS)
        elf=b'fixture';digest=reporter.sha(elf)
        record.update(version=version,repository_commit='new',sha256=digest,size_bytes=len(elf),build_defines=[v for v in command if v.startswith('-D')])
        memory['images']['native'].update(sha256=digest,file_size_bytes=len(elf))
        (root/('settings.elf' if app=='settings' else 'wifi_settings.elf')).write_bytes(elf)
        (root/(app+'-build-record.json')).write_text(json.dumps(record))
        (root/'command.json').write_text(json.dumps(command))
        path=root/'memory.json';path.write_text(json.dumps(memory))
        return path,record,command
    def test_both_apps_preserve_complete_release_contract(self):
        for app in ('settings','wifi'):
            with self.subTest(app=app),tempfile.TemporaryDirectory() as d:
                root=Path(d);memory,_,_=self.fixture(app,root)
                result=reporter.compare(app,root,memory)
                self.assertTrue(result['all_released_features_preserved'])
                self.assertFalse(result['linked_device_memory_fit_qualified'])
                self.assertEqual(result['added_selectors'],sorted(reporter.SELECTORS))
    def test_missing_or_removed_feature_is_rejected(self):
        for app in ('settings','wifi'):
            for missing in (*sorted(reporter.SELECTORS),'-DPORTABLE_QUICK_ACTIONS'):
                with self.subTest(app=app,missing=missing),tempfile.TemporaryDirectory() as d:
                    root=Path(d);memory,record,command=self.fixture(app,root)
                    self.assertIn(missing,command)
                    command.remove(missing);record['build_defines']=[v for v in command if v.startswith('-D')]
                    (root/'command.json').write_text(json.dumps(command));(root/(app+'-build-record.json')).write_text(json.dumps(record))
                    with self.assertRaisesRegex(ValueError,'every released feature'):
                        reporter.compare(app,root,memory)
    def test_missing_capability_or_changed_abi_is_rejected(self):
        for key in ('required_capabilities','imports','exports','external_sources'):
            with self.subTest(key=key),tempfile.TemporaryDirectory() as d:
                root=Path(d);memory,record,_=self.fixture('settings',root)
                record[key]=[] if isinstance(record[key],list) else {}
                (root/'settings-build-record.json').write_text(json.dumps(record))
                with self.assertRaises(ValueError):reporter.compare('settings',root,memory)
if __name__=='__main__':unittest.main()
