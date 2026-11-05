#!/usr/bin/env python3
"""Native alarm selection is explicit, additive and independent of Quick.

The existing Settings --native-ui profile supplies the actual Nova/native
canvas defines. Shared selection does not invent a --native-canvas option or
claim every app builder has a compatible profile; missing C dependencies fail.
"""
import argparse
from contextlib import redirect_stderr
import copy
import hashlib
import io
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import build_portable_settings as settings
import portable_quick_build as shared

ROOT=Path(__file__).resolve().parents[1]
QUICK_SOURCES=[ROOT/'lib/PortableApps/src'/name for name in
               ('quick_actions.c','quick_render.c','quick_session.c')]


class AlarmNativeSelection(unittest.TestCase):
    def arguments(self, *options):
        parser=argparse.ArgumentParser()
        shared.options(parser)
        parser.add_argument('--alarm-client',action='store_true')
        parser.add_argument('--sparse-start',action='store_true')
        parser.add_argument('--nova-ui',action='store_true')
        return parser.parse_args(options),parser

    def configure(self, *options, root=ROOT):
        args,parser=self.arguments(*options)
        with tempfile.TemporaryDirectory() as directory:
            output=Path(directory)
            flags,sources=shared.configure(args,parser,root,output)
            licenses=(output/'licenses/quick-controls').is_dir()
        needs=[{'capability':'display.output','api':1}]
        shared.requirements(args,needs)
        record={'existing':'unchanged','grant_bindings':{'alarm.service':[0]}}
        shared.record(args,record)
        return args,flags,sources,needs,record,licenses

    def assert_rejected(self, message, *options, root=ROOT):
        errors=io.StringIO()
        with redirect_stderr(errors),self.assertRaises(SystemExit) as caught:
            self.configure(*options,root=root)
        self.assertEqual(caught.exception.code,2)
        self.assertIn(message,errors.getvalue())

    def test_disabled_selection_keeps_empty_defaults(self):
        with tempfile.TemporaryDirectory() as directory:
            args,flags,sources,needs,record,licenses=self.configure(root=Path(directory))
        self.assertFalse(args.alarm_native)
        self.assertFalse(args.alarm_client)
        self.assertFalse(args.nova_ui)
        self.assertFalse(args.quick_actions)
        self.assertFalse(args.quick_native)
        self.assertEqual((flags,sources),([],[]))
        self.assertEqual(needs,[{'capability':'display.output','api':1}])
        self.assertEqual(record,{'existing':'unchanged','grant_bindings':{'alarm.service':[0]}})
        self.assertFalse(hasattr(args,'alarm_native_receipt'))
        self.assertFalse(licenses)

    def test_legacy_caller_without_new_attribute_is_unchanged(self):
        args,parser=self.arguments('--alarm-client')
        del args.alarm_native
        with tempfile.TemporaryDirectory() as directory:
            self.assertEqual(shared.configure(args,parser,Path(directory),Path(directory)),([],[]))
        record={'existing':'unchanged'}
        shared.record(args,record)
        self.assertEqual(record,{'existing':'unchanged'})

    def test_disabled_selection_keeps_sparse_profile(self):
        with tempfile.TemporaryDirectory() as directory:
            args,flags,sources,_,record,_=self.configure('--alarm-client','--sparse-start',root=Path(directory))
        self.assertTrue(args.sparse_start)
        self.assertEqual((flags,sources),([],[]))
        self.assertNotIn('alarm_native',record)

    def test_disabled_selection_keeps_paper_quick_contract(self):
        _,flags,sources,needs,record,licenses=self.configure(
            '--alarm-client','--quick-actions','--paper-transitions','--quick-radios')
        self.assertEqual(flags,['-DPORTABLE_PAPER_TRANSITIONS','-DPORTABLE_QUICK_ACTIONS','-DPORTABLE_QUICK_RADIOS'])
        self.assertEqual(sources,QUICK_SOURCES+[ROOT/'lib/PortableApps/src/quick_radios.c'])
        self.assertEqual(needs,[{'capability':name,'api':version} for name,version in
            [('display.output',1),('storage.key-value',1),('rtc.clock',2),('board.battery',1),('net.wifi',1),('bluetooth.hci',1)]])
        self.assertEqual(record,{'existing':'unchanged','grant_bindings':{'alarm.service':[0]}})
        self.assertTrue(licenses)

    def test_native_requires_explicit_alarm_client(self):
        for extra in ((),('--quick-actions',)):
            with self.subTest(extra=extra):
                self.assert_rejected('--alarm-native requires --alarm-client','--alarm-native',*extra)

    def test_native_rejects_sparse_initialization(self):
        for extra in ((),('--quick-actions',)):
            with self.subTest(extra=extra):
                self.assert_rejected('--alarm-native does not support --sparse-start initialization',
                                     '--alarm-client','--alarm-native','--sparse-start',*extra)

    def test_native_requires_each_implementation_source(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            for name in shared.ALARM_NATIVE_SOURCES:
                path=root/name
                path.parent.mkdir(parents=True,exist_ok=True)
                path.write_text(name+'\n')
            for name in shared.ALARM_NATIVE_SOURCES:
                with self.subTest(missing=name):
                    path=root/name
                    path.unlink()
                    self.assert_rejected('--alarm-native requires the native alarm implementation',
                                         '--alarm-client','--alarm-native',root=root)
                    path.write_text(name+'\n')

    def test_native_without_quick_does_not_select_dependencies(self):
        args,flags,sources,needs,record,licenses=self.configure('--alarm-client','--alarm-native')
        self.assertEqual(flags,['-DPORTABLE_ALARM_NATIVE'])
        self.assertEqual(sources,[])
        self.assertEqual(needs,[{'capability':'display.output','api':1}])
        self.assertTrue(args.alarm_client)
        self.assertFalse(args.nova_ui)
        self.assertFalse(getattr(args,'native_canvas',False))
        self.assertFalse(args.quick_actions)
        self.assertFalse(args.quick_native)
        self.assertFalse(args.quick_radios)
        self.assertFalse(licenses)
        self.assertEqual(set(record),{'existing','grant_bindings','alarm_native'})

    def test_native_is_only_an_additive_define_and_receipt(self):
        for extra in ((),('--nova-ui',),('--raster-snapshot',),('--quick-actions',),
                      ('--quick-actions','--quick-native'),
                      ('--quick-actions','--paper-transitions'),
                      ('--quick-actions','--quick-radios','--quick-native','--raster-snapshot')):
            with self.subTest(extra=extra):
                base=('--alarm-client',*extra)
                ordinary=self.configure(*base)
                native=self.configure(*base,'--alarm-native')
                self.assertEqual(native[1].count('-DPORTABLE_ALARM_NATIVE'),1)
                self.assertEqual([flag for flag in native[1] if flag!='-DPORTABLE_ALARM_NATIVE'],ordinary[1])
                self.assertEqual(native[2:4],ordinary[2:4])
                self.assertEqual({k:v for k,v in native[4].items() if k!='alarm_native'},ordinary[4])
                self.assertEqual(native[5],ordinary[5])
                self.assertEqual({k:v for k,v in vars(native[0]).items()
                                  if k not in ('alarm_native','alarm_native_receipt')},
                                 {k:v for k,v in vars(ordinary[0]).items() if k!='alarm_native'})
                self.assertNotIn('-DPORTABLE_NATIVE_CANVAS',native[1])
                self.assertNotIn('-DPORTABLE_NOVA_UI',native[1])
                self.assertNotIn('-DPORTABLE_WATCH_NATIVE_HEIGHT',native[1])

    def test_native_keeps_existing_requirements_and_versions(self):
        ordinary,_=self.arguments('--alarm-client')
        native,_=self.arguments('--alarm-client','--alarm-native')
        before=[{'capability':name,'api':api} for name,api in
                [('display.output',1),('alarm.service',2),('storage.key-value',1)]]
        after=copy.deepcopy(before)
        shared.requirements(ordinary,before)
        shared.requirements(native,after)
        self.assertEqual(before,after)
        for app in ('paper_clock','springboard','settings','file_browser','wifi_settings',
                    'ota_update','app_store','usb_sd_transfer'):
            with self.subTest(app=app):
                self.assertEqual(shared.version(native,app,'1.2.3'),shared.version(ordinary,app,'1.2.3'))
        self.assertEqual(shared.exports(native,{'app_main'}),shared.exports(ordinary,{'app_main'}))

    def test_native_record_binds_implementation_sources(self):
        _,_,_,_,record,_=self.configure('--alarm-client','--alarm-native')
        receipt=record['alarm_native']
        self.assertTrue(receipt['selected'])
        self.assertFalse(receipt['grants_added'])
        self.assertFalse(receipt['hardware_verified'])
        self.assertEqual(receipt['input_coordinates'],'native-provider')
        hashes=receipt['source_sha256']
        self.assertTrue({
            'scripts/portable_quick_build.py','lib/PortableApps/src/adapter.c',
            'lib/PortableApps/src/foreground_adapter_open.inc','lib/PortableApps/src/alarm.inc',
            'lib/PortableApps/include/PortableAlarmLayout.h','lib/PortableApps/src/alarm_native.inc',
            'lib/PortableApps/include/PortableNativeCanvas.h','lib/PortableApps/include/PortableNovaUi.h',
            'lib/PortableApps/src/nova_ui.inc','lib/PortableApps/src/nova_native.inc',
            'lib/PortableApps/src/raster_snapshot_replay.inc'
        }.issubset(hashes))
        self.assertEqual(len(hashes),len(shared.ALARM_NATIVE_SOURCES))
        self.assertEqual(hashes,{name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest()
                                 for name in shared.ALARM_NATIVE_SOURCES})

    def settings_command(self, *options):
        class CommandCaptured(Exception):
            pass
        commands=[]
        def capture(command, **kwargs):
            commands.append(command)
            raise CommandCaptured
        parser=settings.argument_parser()
        with tempfile.TemporaryDirectory() as directory:
            args=parser.parse_args([*options,'--output-dir',directory])
            with patch.object(settings,'resolve_compiler',return_value='cc'), \
                    patch.object(settings.subprocess,'run',side_effect=capture), \
                    self.assertRaises(CommandCaptured):
                settings.build(args,parser)
        self.assertEqual(len(commands),1)
        return args,commands[0]

    def compile_adapter(self, flags):
        return subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
            '-fsyntax-only',*flags,'-I'+str(ROOT/'lib/PortableApps/include'),
            '-I'+str(ROOT/'lib/NativeApps/include'),str(ROOT/'lib/PortableApps/src/adapter.c')],
            text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=60)

    def test_existing_native_settings_profile_supplies_c_dependencies(self):
        for extra in ((),('--quick-actions','--quick-native','--raster-snapshot')):
            with self.subTest(extra=extra):
                base=('--native-ui','--alarm-client',*extra)
                ordinary_args,ordinary=self.settings_command(*base)
                native_args,native=self.settings_command(*base,'--alarm-native')
                ordinary_flags=[flag for flag in ordinary if flag.startswith('-D')]
                native_flags=[flag for flag in native if flag.startswith('-D')]
                self.assertEqual([flag for flag in native_flags if flag!='-DPORTABLE_ALARM_NATIVE'],ordinary_flags)
                self.assertTrue(native_args.native_ui)
                self.assertFalse(native_args.nova_ui)
                self.assertFalse(hasattr(native_args,'native_canvas'))
                self.assertFalse(ordinary_args.alarm_native)
                self.assertEqual(native_flags.count('-DPORTABLE_ALARM_NATIVE'),1)
                for dependency in ('PORTABLE_ALARM_CLIENT','PORTABLE_NOVA_UI','PORTABLE_NATIVE_CANVAS'):
                    self.assertIn('-D'+dependency,native_flags)
                if not extra:self.assertNotIn('-DPORTABLE_QUICK_ACTIONS',native_flags)
                compiled=self.compile_adapter(native_flags)
                self.assertEqual(compiled.returncode,0,compiled.stderr)

    def test_compile_guards_reject_each_missing_dependency(self):
        dependencies=['PORTABLE_ALARM_CLIENT','PORTABLE_NOVA_UI','PORTABLE_NATIVE_CANVAS']
        for missing in dependencies:
            with self.subTest(missing=missing):
                compiled=self.compile_adapter(['-DPORTABLE_ALARM_NATIVE',
                    *('-D'+name for name in dependencies if name!=missing)])
                self.assertNotEqual(compiled.returncode,0)
                self.assertIn('Native alarm requires explicitly selected alarm client, Nova UI and native canvas',
                              compiled.stderr)

    def test_compile_guard_rejects_sparse_initialization(self):
        compiled=self.compile_adapter(['-D'+name for name in ('PORTABLE_ALARM_NATIVE',
            'PORTABLE_ALARM_CLIENT','PORTABLE_NOVA_UI','PORTABLE_NATIVE_CANVAS',
            'PORTABLE_DESK_CLOCK_SPARSE_START')])
        self.assertNotEqual(compiled.returncode,0)
        self.assertIn('Native alarm requires generic foreground initialization',compiled.stderr)


if __name__=='__main__':unittest.main()
