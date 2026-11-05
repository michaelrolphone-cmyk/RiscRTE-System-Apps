#!/usr/bin/env python3
"""Native Quick is additive, explicit, source-bound and capability-neutral."""
import argparse
from contextlib import redirect_stderr
import hashlib
import io
from pathlib import Path
import tempfile
import unittest

import portable_quick_build as shared

ROOT=Path(__file__).resolve().parents[1]
QUICK_SOURCES=[ROOT/'lib/PortableApps/src'/name for name in
               ('quick_actions.c','quick_render.c','quick_session.c')]


class QuickNativeSelection(unittest.TestCase):
    def arguments(self, *options):
        parser=argparse.ArgumentParser()
        shared.options(parser)
        parser.add_argument('--alarm-client',action='store_true')
        parser.add_argument('--sparse-start',action='store_true')
        return parser.parse_args(options),parser

    def configure(self, *options, root=ROOT):
        args,parser=self.arguments(*options)
        with tempfile.TemporaryDirectory() as directory:
            flags,sources=shared.configure(args,parser,root,Path(directory))
        needs=[{'capability':'display.output','api':1}]
        shared.requirements(args,needs)
        record={'existing':'unchanged'}
        shared.record(args,record)
        return args,flags,sources,needs,record

    def assert_rejected(self, message, *options, root=ROOT):
        errors=io.StringIO()
        with redirect_stderr(errors),self.assertRaises(SystemExit) as caught:
            self.configure(*options,root=root)
        self.assertEqual(caught.exception.code,2)
        self.assertIn(message,errors.getvalue())

    def test_disabled_selection_keeps_empty_defaults(self):
        with tempfile.TemporaryDirectory() as directory:
            args,flags,sources,needs,record=self.configure(root=Path(directory))
        self.assertFalse(args.quick_native)
        self.assertEqual((flags,sources),([],[]))
        self.assertEqual(needs,[{'capability':'display.output','api':1}])
        self.assertEqual(record,{'existing':'unchanged'})
        self.assertFalse(hasattr(args,'quick_native_receipt'))

    def test_disabled_selection_keeps_existing_quick_contract(self):
        _,flags,sources,needs,record=self.configure('--alarm-client','--quick-actions','--quick-radios')
        self.assertEqual(flags,['-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_QUICK_RADIOS'])
        self.assertEqual(sources,QUICK_SOURCES+[ROOT/'lib/PortableApps/src/quick_radios.c'])
        self.assertEqual(needs,[{'capability':name,'api':version} for name,version in
            [('display.output',1),('storage.key-value',1),('rtc.clock',2),('board.battery',1),('net.wifi',1),('bluetooth.hci',1)]])
        self.assertEqual(record,{'existing':'unchanged'})

    def test_native_requires_explicit_quick_actions(self):
        self.assert_rejected('--quick-native requires --quick-actions','--alarm-client','--quick-native')

    def test_native_preserves_alarm_requirement(self):
        self.assert_rejected('--quick-actions requires --alarm-client','--quick-actions','--quick-native')

    def test_native_rejects_sparse_initialization(self):
        self.assert_rejected('--quick-native does not support --sparse-start initialization',
                             '--alarm-client','--quick-actions','--quick-native','--sparse-start')

    def test_native_requires_selected_implementation(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assert_rejected('--quick-native requires the native Quick implementation',
                                 '--alarm-client','--quick-actions','--quick-native',root=Path(directory))

    def test_native_is_only_an_additive_define(self):
        for extra in ((),('--quick-radios',),('--raster-snapshot',)):
            with self.subTest(extra=extra):
                base=('--alarm-client','--quick-actions',*extra)
                ordinary=self.configure(*base)
                native=self.configure(*base,'--quick-native')
                self.assertEqual(native[1].count('-DPORTABLE_QUICK_NATIVE'),1)
                self.assertEqual([flag for flag in native[1] if flag!='-DPORTABLE_QUICK_NATIVE'],ordinary[1])
                self.assertEqual(native[2:4],ordinary[2:4])
                self.assertEqual({k:v for k,v in native[4].items() if k!='quick_native'},ordinary[4])
                self.assertNotIn('-DPORTABLE_NATIVE_CANVAS',native[1])
                self.assertNotIn('-DPORTABLE_WATCH_NATIVE_HEIGHT',native[1])

    def test_native_record_binds_implementation_sources(self):
        _,_,sources,_,record=self.configure('--alarm-client','--quick-actions','--quick-native')
        self.assertEqual(sources,QUICK_SOURCES)
        receipt=record['quick_native']
        self.assertTrue(receipt['selected'])
        self.assertFalse(receipt['grants_added'])
        self.assertFalse(receipt['hardware_verified'])
        self.assertEqual(receipt['input_coordinates'],'native-provider')
        hashes=receipt['source_sha256']
        self.assertTrue({
            'scripts/portable_quick_build.py','lib/PortableApps/src/adapter.c',
            'lib/PortableApps/src/foreground_adapter_open.inc','lib/PortableApps/src/quick_adapter.inc',
            'lib/PortableApps/include/PortableQuickLayout.h','lib/PortableApps/src/quick_native_render.inc',
            'lib/PortableApps/src/quick_native_fonts.inc','lib/PortableApps/src/raster_snapshot_replay.inc'
        }.issubset(hashes))
        self.assertEqual(hashes,{name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in shared.QUICK_NATIVE_SOURCES})


if __name__=='__main__':unittest.main()
