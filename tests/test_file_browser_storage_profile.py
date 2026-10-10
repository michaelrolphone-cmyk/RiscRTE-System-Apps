import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from build_portable_file_browser import storage_options, operation_options

class FileBrowserStorageProfile(unittest.TestCase):
    def args(self,**changes):
        values=dict(storage_capability='storage.installed-files',storage_instance=0,secondary_storage_capability='storage.volume',secondary_storage_instance=None)
        values.update(changes)
        return SimpleNamespace(**values)
    def test_default_unchanged(self):
        flags,caps=storage_options(self.args())
        self.assertEqual(caps,['storage.installed-files'])
        self.assertEqual(len(flags),2)
    def test_watch_keeps_installed_files(self):
        flags,caps=storage_options(self.args(secondary_storage_capability='storage.app-data.export',secondary_storage_instance=0))
        self.assertEqual(caps,['storage.installed-files','storage.app-data.export'])
        self.assertIn('-DPORTABLE_FILE_BROWSER_SECONDARY_CAPABILITY="storage.app-data.export"',flags)
        self.assertIn('-DPORTABLE_FILE_BROWSER_SECONDARY_INSTANCE=0u',flags)
    def test_same_capability_requires_distinct_instances(self):
        flags,caps=storage_options(self.args(storage_capability='storage.volume',storage_instance=11,secondary_storage_instance=22))
        self.assertEqual(caps,['storage.volume'])
        self.assertIn('-DPORTABLE_FILE_BROWSER_SECONDARY_INSTANCE=22u',flags)
        for secondary in (0,11):
            with self.assertRaises(ValueError):storage_options(self.args(storage_capability='storage.volume',storage_instance=11,secondary_storage_instance=secondary))
    def test_invalid_and_unselected(self):
        for changes in ({'secondary_storage_capability':'storage.app-data.export'}, {'secondary_storage_instance':0}, {'secondary_storage_instance':-1}, {'secondary_storage_instance':2**31}, {'storage_capability':'x"junk'}, {'secondary_storage_capability':'bad/name'}):
            with self.assertRaises(ValueError):storage_options(self.args(**changes))

    def test_optional_operation_profiles(self):
        self.assertEqual(operation_options(SimpleNamespace(file_handlers=False)),[])
        self.assertEqual(operation_options(SimpleNamespace(file_handlers=False,rgb_only=True,read_copy_only=True)),['-DPORTABLE_FILE_BROWSER_READ_COPY_ONLY','-DPORTABLE_FILE_BROWSER_RGB_ONLY'])
        with self.assertRaises(ValueError):operation_options(SimpleNamespace(file_handlers=True,read_copy_only=True))

if __name__=='__main__':unittest.main()
