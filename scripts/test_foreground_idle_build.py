#!/usr/bin/env python3
"""Host-only qualification of opt-in inputs, namespace preservation and SDK custody."""
import argparse
import contextlib
import copy
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import portable_foreground_idle_build as idle
from build_portable_settings import argument_parser


class ForegroundIdleBuildTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='foreground-idle-build-')
        self.base = Path(self.temporary.name)
        self.root, self.out = self.base/'system', self.base/'out'
        self.include = self.root/'lib/PortableApps/include'
        self.include.mkdir(parents=True)
        (self.include.parent/'time').mkdir()
        (self.include.parent/'time/time.h').write_text('#define TIME_TEST 7\n')
        for name in idle.REQUIRED_HEADERS:
            (self.include/name).write_text('/* bundled ' + name + ' */\n')
        (self.include/'RiscRuntimeV1.h').write_text('#define RUNTIME_TEST 1\n')
        (self.include/'PortableForegroundIdle.h').write_text(
            '#include "RiscRuntimeV1.h"\n#include "../time/time.h"\n')
        self.helpers, self.drivers = self.base/'helpers', self.base/'drivers'
        self.helpers.mkdir(); self.drivers.mkdir()
        for name in idle.SOURCE_NAMES:
            (self.helpers/name).write_text('/* ' + name + ' */\n')
        (self.drivers/'RiscRuntimeV1.h').write_text('#define RUNTIME_TEST 2\n')
        (self.drivers/'RiscProviderV2.h').write_text('#define PROVIDER_TEST 2\n')
        (self.drivers/'RiscDisplayPowerV1.h').write_text('#define DISPLAY_TEST 3\n')
        self.canonical = {'LICENSE': b'Runtime license',
            'sdk/app/RiscRuntimeV1.h': b'#define RUNTIME_TEST 4\n',
            'sdk/driver/RiscProviderV2.h': b'#define PROVIDER_TEST 4\n',
            'sdk/hardware/RiscHardwareConfigV1.h': b'/* runtime hardware */\n'}
        self.parser = argparse.ArgumentParser()
        idle.options(self.parser)
        self.parser.add_argument('--navigation', action='store_true')
        self.cli = ['--foreground-idle-helper-dir', str(self.helpers),
            '--foreground-idle-runtime-dir', str(self.base/'runtime'),
            '--foreground-idle-drivers-sdk-dir', str(self.drivers), '--navigation']

    def tearDown(self):
        self.temporary.cleanup()

    def configure(self, extra=()):
        args = self.parser.parse_args([*self.cli, *extra])
        with patch.object(idle, '_runtime_headers', return_value=self.canonical), \
                patch.object(idle, 'verify_runtime_sdk', return_value={'fixture': True}), \
                patch.object(idle, '_repository', return_value=None):
            result = idle.configure(args, self.parser, self.root, self.out, self.include)
        return args, result

    def reject(self, argv):
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            idle.validate(self.parser.parse_args(argv), self.parser)

    def test_unselected_native_recipe_does_not_grant_foreground_capabilities(self):
        args = self.parser.parse_args([])
        source = self.base/'native.c'
        source.write_text('int native_entry(void){return 0;}\n')
        command = ['target-gcc', '-DNATIVE_UI', '-I'+str(self.include), str(source), '-o', str(self.out/'native.elf')]
        record = {'grant_bindings': {'storage.key-value': [5]}}
        idle.record(args, record, ['-DNATIVE_UI'], compile_command=command)
        self.assertEqual(record['compile_command'], command)
        self.assertEqual(record['compile_inputs']['sources'], [str(source)])
        self.assertEqual(record['compile_inputs']['source_sha256'][str(source)], hashlib.sha256(source.read_bytes()).hexdigest())
        self.assertEqual(record['compile_inputs']['definitions'], ['-DNATIVE_UI'])
        self.assertEqual(record['compile_inputs']['include_directories'], [str(self.include)])
        self.assertNotIn('foreground_idle', record)
        self.assertEqual(record['grant_bindings'], {'storage.key-value': [5]})

    def test_omission_has_no_io_flags_sources_or_requirements(self):
        args = self.parser.parse_args([])
        with patch.object(idle, '_runtime_headers', side_effect=AssertionError('unexpected read')):
            self.assertEqual(idle.configure(args, self.parser, self.root, self.out, self.include),
                             (self.include, [], []))
        self.assertFalse(self.out.exists())
        manifest = [{'capability':'display.output', 'api':1}]
        original = copy.deepcopy(manifest)
        idle.requirements(args, manifest)
        self.assertEqual(manifest, original)
        record = {'unchanged': True}
        idle.record(args, record, ['-DOLD'])
        self.assertEqual(record, {'unchanged': True})

    def test_partial_options_and_conflicting_owners_are_rejected(self):
        for argv in (self.cli[:2], self.cli[2:4], self.cli[4:], self.cli[:4],
                     ['--foreground-idle-fallback-seconds','15']):
            self.reject(argv)
        for name in ('x4_idle_source','resident_shell_host','resident_shell_client',
                     'desk_lock_home','sparse_start'):
            args = self.parser.parse_args(self.cli)
            setattr(args, name, True)
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                idle.validate(args, self.parser)

    def test_ready_drain_requires_tagged_foreground_and_records_budget(self):
        self.assertIs(idle.ALARM_READY_DRAIN_SUPPORTED, True)
        for argv in (['--alarm-ready-drain'], [*self.cli, '--alarm-ready-drain'],
                     [*self.cli, '--foreground-alarm-sleep', '--alarm-ready-drain']):
            self.reject(argv)
        args = self.parser.parse_args([*self.cli, '--foreground-alarm-sleep', '--alarm-ready-drain'])
        args.alarm_client = True
        args.tagged_alarm_utilities = self.base/'utilities'
        def stage(args, parser, out, includes):
            for name in ('AlarmServiceV1.h', 'AlarmServiceV2.h'):
                (includes/name).write_text('/* tagged fixture */')
            return {'commit': 'a'*40}
        with patch.object(idle, '_runtime_headers', return_value=self.canonical), \
                patch.object(idle, 'verify_runtime_sdk', return_value={'fixture': True}), \
                patch.object(idle, '_repository', return_value=None), \
                patch('portable_alarm_build.stage', side_effect=stage):
            _, flags, _ = idle.configure(args, self.parser, self.root, self.out, self.include)
        self.assertIn('-DPORTABLE_ALARM_READY_DRAIN', flags)
        self.assertIn('-DPORTABLE_ALARM_READY_BUDGET_MS=8u', flags)
        self.assertIn('-DALARM_SERVICE_TAGGED_V2', flags)
        self.assertEqual(args.foreground_idle_receipt['alarm_ready_drain']['budget_ms'], 8)
        record = {}
        idle.record(args, record, flags)
        self.assertTrue(record['foreground_idle']['alarm_ready_drain']['enabled'])
        omitted, (_, omitted_flags, _) = self.configure()
        self.assertNotIn('-DPORTABLE_ALARM_READY_DRAIN', omitted_flags)
        self.assertNotIn('alarm_ready_drain', omitted.foreground_idle_receipt)

    def test_alarm_protocol_requires_explicit_tagged_client(self):
        self.reject(['--foreground-alarm-sleep'])
        self.reject([*self.cli, '--foreground-alarm-sleep'])
        args = self.parser.parse_args([*self.cli, '--foreground-alarm-sleep'])
        args.alarm_client=True
        args.tagged_alarm_utilities=self.base/'utilities'
        def stage(args, parser, out, includes):
            for name in ('AlarmServiceV1.h', 'AlarmServiceV2.h'):
                (includes/name).write_text('/* pinned SDK fixture */')
            return {'commit': 'a'*40}
        with patch.object(idle, '_runtime_headers', return_value=self.canonical), \
                patch.object(idle, 'verify_runtime_sdk', return_value={'fixture': True}), \
                patch.object(idle, '_repository', return_value=None), \
                patch('portable_alarm_build.stage', side_effect=stage):
            _, flags, _ = idle.configure(args, self.parser, self.root, self.out, self.include)
        self.assertIn('-DPORTABLE_FOREGROUND_ALARM_SLEEP', flags)
        self.assertEqual(args.foreground_idle_receipt['alarm_sleep']['api'], 2)
        needs=[{'capability':'alarm.service','api':1}]
        idle.requirements(args, needs)
        self.assertEqual([row for row in needs if row['capability']=='alarm.service'],
                         [{'capability':'alarm.service','api':2}])

    def test_navigation_lifetime_contract_is_required(self):
        self.reject([argument for argument in self.cli if argument != '--navigation'])
        args, _ = self.configure()
        self.assertTrue(args.foreground_idle_receipt['prerequisites']['adapter_input_navigation'])

    def test_fallback_bounds_and_selected_flags(self):
        for value in ('4','3601','0','-1','nan','1.5'):
            self.reject([*self.cli,'--foreground-idle-fallback-seconds',value])
        for value in (None, 5, 15, 60, 3600):
            extra = () if value is None else ('--foreground-idle-fallback-seconds',str(value))
            args, (include, flags, sources) = self.configure(extra)
            self.assertEqual(flags, [*idle.FLAGS,
                '-DPORTABLE_FOREGROUND_IDLE_FALLBACK_SECONDS='+str(value or 15)+'u'])
            self.assertEqual(sources, [self.helpers/name for name in idle.SOURCE_NAMES])
            self.assertEqual(args.foreground_idle_receipt['fallback_seconds'],value or 15)
            self.assertEqual(args.foreground_idle_receipt['mode'],'returning-light-only')

    def test_canonical_quoted_headers_win_without_source_edits(self):
        before = {p: p.read_bytes() for p in self.include.rglob('*') if p.is_file()}
        args, (include, flags, sources) = self.configure()
        self.assertEqual((include/'RiscRuntimeV1.h').read_bytes(),self.canonical['sdk/app/RiscRuntimeV1.h'])
        self.assertEqual((include/'RiscProviderV2.h').read_bytes(),self.canonical['sdk/driver/RiscProviderV2.h'])
        self.assertEqual((include/'RiscDisplayPowerV1.h').read_bytes(),(self.drivers/'RiscDisplayPowerV1.h').read_bytes())
        self.assertEqual(before, {p:p.read_bytes() for p in before})
        source = self.base/'quoted.c'
        source.write_text('#include "PortableForegroundIdle.h"\n#include "RiscProviderV2.h"\n'
            '_Static_assert(RUNTIME_TEST == 4, "stale quoted Runtime header");\n'
            '_Static_assert(PROVIDER_TEST == 4, "stale provider header");\n'
            '_Static_assert(TIME_TEST == 7, "time sibling missing");\n')
        subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(include),
                        '-c',str(source),'-o',str(self.base/'quoted.o')],check=True)
        origins = args.foreground_idle_receipt['header_origins']
        self.assertEqual(origins['RiscRuntimeV1.h']['layer'],'runtime-app')
        self.assertEqual(origins['RiscProviderV2.h']['layer'],'runtime-driver')
        self.assertEqual(origins['RiscDisplayPowerV1.h']['layer'],'drivers-capability')
        for name,item in origins.items():
            self.assertEqual(item['sha256'],hashlib.sha256((include/name).read_bytes()).hexdigest())
        # Repeat selection cannot retain stray generated SDK files.
        (include/'stale.h').write_text('stale')
        (include.parent/'time/stale.h').write_text('stale')
        self.configure()
        self.assertFalse((include/'stale.h').exists())
        self.assertFalse((include.parent/'time/stale.h').exists())

    def test_private_namespaces_are_additive_never_wildcard(self):
        args, (_,flags,_) = self.configure()
        for private, expected in (((),[1]),((2,),[2,1]),((6,),[6,1])):
            record={'grant_bindings':{'storage.key-value':list(private)},
                    'required_grants':[{'capability':'power.sleep','api':1,'instance_id':0}]}
            idle.record(args,record,flags,private_kv=private,compile_command=['cc',*flags])
            self.assertEqual(record['grant_bindings']['storage.key-value'],expected)
            self.assertNotIn('instance_id',record['required_grants'][0])
            self.assertEqual([r['instance_id'] for r in record['required_grants']
                if r['capability']=='storage.key-value'],expected)
            self.assertFalse(record['foreground_idle']['preferences']['grant_is_readonly'])
            self.assertEqual(record['compile_command'],['cc',*flags])
        with self.assertRaises(ValueError):
            idle.record(args,{'grant_bindings':{'storage.key-value':[0]}},flags)
        needs=[{'capability':'storage.key-value','api':1}]
        idle.requirements(args,needs);idle.requirements(args,needs)
        self.assertEqual(len(needs),4)
        self.assertEqual({r['capability'] for r in needs},{*idle.CAPABILITIES,'storage.key-value'})

    def test_staging_cannot_be_inside_or_replace_source_tree(self):
        args = self.parser.parse_args(self.cli)
        with patch.object(idle, '_runtime_headers', return_value=self.canonical), \
                patch.object(idle, 'verify_runtime_sdk', return_value={'fixture': True}), \
                contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            idle.configure(args, self.parser, self.root, self.include, self.include)

    def test_missing_sources_and_wrong_runtime_head_reject(self):
        (self.helpers/idle.SOURCE_NAMES[-1]).unlink()
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            self.configure()
        with patch.object(idle.subprocess,'check_output',return_value='wrong-head\n'), \
                contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            idle._runtime_headers(self.base/'runtime',self.parser)

    def test_native_subset_receipt_uses_exact_selected_runtime(self):
        args = self.parser.parse_args(self.cli)
        with patch.object(idle, '_runtime_headers', return_value=self.canonical):
            sdk = idle.runtime_app_sdk(args, self.parser, ('RiscRuntimeV1.h',))
        self.assertEqual(sdk, {'RiscRuntimeV1.h': self.canonical['sdk/app/RiscRuntimeV1.h'],
                               'LICENSE': self.canonical['LICENSE']})

    def test_settings_does_not_tie_light_to_home_mode_choice(self):
        parser = argument_parser()
        args = parser.parse_args([*self.cli,'--settings-profile','native-time-touch',
                                  '--sleep-settings','--default-sleep-mode','deep'])
        self.assertTrue(idle.validate(args, parser))
        self.assertEqual(args.default_sleep_mode,'deep')


if __name__ == '__main__':
    unittest.main()
