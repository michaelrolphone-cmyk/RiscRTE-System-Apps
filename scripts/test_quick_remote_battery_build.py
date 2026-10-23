#!/usr/bin/env python3
"""T061 local opt-in selection, exact source/SDK receipts and default stability.

These configuration tests do not compile an ELF, grant a capability, select a
device, alter an accepted product pin, publish or access hardware.
"""
import argparse
import ast
from contextlib import redirect_stderr
import hashlib
import io
import json
from pathlib import Path
import shutil
import tempfile
import unittest

import portable_quick_build as shared

ROOT=Path(__file__).resolve().parents[1]
HOST_OPTIONS=('--resident-shell-host','--alarm-client','--quick-actions',
              '--quick-radios','--paper-transitions')
SELECT=('--quick-remote-battery','--quick-remote-battery-version','0.4.21')
CAPABILITY={'capability':'bluetooth.sensors','api':1}


class RemoteBatteryBuild(unittest.TestCase):
    def setUp(self):
        self.temporary=tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory=Path(self.temporary.name)
        self.sdk=self.directory/'runtime-sdk';self.sdk.mkdir()
        # The fixture exercises configuration validation only, not C ABI layout.
        (self.sdk/'RiscRuntimeV1.h').write_text('#define RISC_RUNTIME_RESIDENT_SHELL_V1_SIZE 1\n')
        (self.sdk/'RiscResidentShellV1.h').write_text('/* configuration fixture */\n')
        self.includes=self.directory/'staged';self.includes.mkdir()
        (self.includes/'RiscDisplayOutputSnapshotV1.h').write_text('/* configuration fixture */\n')
        shutil.copyfile(ROOT/'lib/PortableApps/include'/shared.QUICK_REMOTE_BATTERY_SDK,
                        self.includes/shared.QUICK_REMOTE_BATTERY_SDK)

    def arguments(self,*options):
        parser=argparse.ArgumentParser()
        shared.options(parser)
        parser.add_argument('--alarm-client',action='store_true')
        args=parser.parse_args(options)
        args.resident_runtime_sdk=self.sdk
        return args,parser

    def configure(self,*options,root=ROOT,includes=True):
        args,parser=self.arguments(*options)
        flags,sources=shared.configure(args,parser,root,self.directory/'output',
                                      self.includes if includes else None)
        needs=[{'capability':'display.output','api':1}]
        shared.requirements(args,needs)
        record={'existing':'unchanged'};shared.record(args,record)
        return args,flags,sources,needs,record

    def assert_rejected(self,message,*options,**kwargs):
        errors=io.StringIO()
        with redirect_stderr(errors),self.assertRaises(SystemExit) as caught:
            self.configure(*options,**kwargs)
        self.assertEqual(caught.exception.code,2)
        self.assertIn(message,errors.getvalue())

    def test_disabled_defaults_need_no_remote_sources_or_sdk(self):
        args,flags,sources,needs,record=self.configure(root=self.directory,includes=False)
        self.assertFalse(args.quick_remote_battery)
        self.assertFalse(hasattr(args,'quick_remote_battery_receipt'))
        self.assertEqual((flags,sources),([],[]))
        self.assertEqual(needs,[{'capability':'display.output','api':1}])
        self.assertEqual(record,{'existing':'unchanged'})

    def test_each_prerequisite_must_be_explicit(self):
        for missing in ('--resident-shell-host','--quick-actions','--quick-radios','--paper-transitions'):
            with self.subTest(missing=missing):
                self.assert_rejected('--quick-remote-battery requires',
                                     *[option for option in HOST_OPTIONS if option!=missing],*SELECT)

    def test_remote_does_not_select_host_quick_or_radios_implicitly(self):
        args,_=self.arguments(*SELECT)
        self.assertFalse(args.resident_shell_host)
        self.assertFalse(args.quick_actions)
        self.assertFalse(args.quick_radios)
        self.assertFalse(args.paper_transitions)
        self.assert_rejected('--quick-remote-battery requires',*SELECT)

    def test_remote_preserves_alarm_client_requirement(self):
        self.assert_rejected('Resident role requires --alarm-client',
                             *[option for option in HOST_OPTIONS if option!='--alarm-client'],*SELECT)

    def test_explicit_version_is_required_and_cannot_be_selected_alone(self):
        self.assert_rejected('must be selected together',*HOST_OPTIONS,'--quick-remote-battery')
        self.assert_rejected('must be selected together','--quick-remote-battery-version','0.4.21')
        self.assert_rejected('must be selected together','--quick-remote-battery-version=')

    def test_explicit_version_matches_runtime_numeric_grammar(self):
        for version in ('','0.4.21-dev1','0.4','0.4.21+build','-1.4.21','0.4.4294967296','9'*32+'.0.0'):
            with self.subTest(version=version):
                self.assert_rejected('Runtime-compatible numeric',*HOST_OPTIONS,
                                     '--quick-remote-battery','--quick-remote-battery-version='+version)

    def test_local_version_must_exceed_final_selected_ordinary_profile(self):
        args,parser=self.arguments(*HOST_OPTIONS,*SELECT)
        self.assertEqual(shared.remote_battery_version(args,parser,'0.4.20'),'0.4.21')
        for ordinary in ('0.4.21','0.4.22','1.0.0'):
            errors=io.StringIO()
            with redirect_stderr(errors),self.assertRaises(SystemExit):
                shared.remote_battery_version(args,parser,ordinary)
            self.assertIn('must be newer than the selected ordinary profile version '+ordinary,errors.getvalue())
        args.quick_remote_battery=False
        self.assertEqual(shared.remote_battery_version(args,parser,'0.4.20'),'0.4.20')

    def test_remote_rejects_resident_clients(self):
        self.assert_rejected('resident clients are unsupported',
                             '--resident-shell-client','--alarm-client',*SELECT)
        self.assert_rejected('resident clients are unsupported',
                             '--resident-shell-client','--alarm-client','--quick-actions',
                             '--quick-radios','--paper-transitions',*SELECT)

    def test_remote_requires_complete_source_set(self):
        self.assert_rejected('requires the remote battery implementation',
                             *HOST_OPTIONS,*SELECT,root=self.directory)

    def test_remote_requires_staged_canonical_header(self):
        (self.includes/shared.QUICK_REMOTE_BATTERY_SDK).unlink()
        self.assert_rejected('requires the canonical copied and staged',*HOST_OPTIONS,*SELECT)
        self.assert_rejected('requires the canonical copied and staged',*HOST_OPTIONS,*SELECT,includes=False)

    def test_remote_rejects_modified_staged_contract(self):
        with (self.includes/shared.QUICK_REMOTE_BATTERY_SDK).open('a') as file:
            file.write('\n/* incompatible staged source */\n')
        self.assert_rejected('SDK differs from the exact canonical',*HOST_OPTIONS,*SELECT)

    def test_remote_rejects_modified_source_contract(self):
        root=self.directory/'modified-source'
        for name in shared.QUICK_REMOTE_BATTERY_SOURCES:
            path=root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text(name+'\n')
        (root/'lib/PortableApps/include'/shared.QUICK_REMOTE_BATTERY_SDK).write_text('/* wrong SDK */\n')
        self.assert_rejected('SDK differs from the exact canonical',*HOST_OPTIONS,*SELECT,root=root)

    def test_remote_is_exactly_one_additive_define_source_and_optional_declaration(self):
        ordinary=self.configure(*HOST_OPTIONS)
        remote=self.configure(*HOST_OPTIONS,*SELECT)
        self.assertEqual(remote[1],ordinary[1]+['-DPORTABLE_QUICK_REMOTE_BATTERY'])
        self.assertEqual(remote[2],ordinary[2]+[ROOT/'lib/PortableApps/src/remote_battery.c'])
        self.assertEqual(remote[3],ordinary[3])
        normal_optional=[];remote_optional=[]
        shared.optional_requirements(ordinary[0],normal_optional)
        shared.optional_requirements(remote[0],remote_optional)
        self.assertEqual(normal_optional,[])
        self.assertEqual(remote_optional,[CAPABILITY])
        self.assertNotIn('quick_remote_battery',ordinary[4])
        self.assertNotIn('grant_bindings',ordinary[4])
        self.assertNotIn('grant_bindings',remote[4])
        self.assertEqual({k:v for k,v in remote[4].items() if k not in ('quick_remote_battery','optional_grant_bindings')},ordinary[4])
        self.assertEqual(remote[4]['optional_grant_bindings'],{'bluetooth.sensors':[0]})
        self.assertEqual(shared.exports(remote[0],{'app_main'}),shared.exports(ordinary[0],{'app_main'}))

    def test_optional_declaration_is_not_duplicated(self):
        args,_=self.arguments(*HOST_OPTIONS,*SELECT)
        needs=[dict(CAPABILITY)]
        shared.optional_requirements(args,needs)
        self.assertEqual(needs.count(CAPABILITY),1)

    def test_receipt_hashes_all_integration_inputs_and_exact_sdk(self):
        *_,record=self.configure(*HOST_OPTIONS,*SELECT)
        receipt=record['quick_remote_battery']
        required={
            'lib/PortableApps/src/adapter.c','lib/PortableApps/src/quick_adapter.inc',
            'lib/PortableApps/src/quick_remote_battery.inc','lib/PortableApps/src/quick_paper.inc',
            'lib/PortableApps/src/quick_reference.inc','lib/PortableApps/src/quick_atomic.inc',
            'lib/PortableApps/src/quick_actions.c','lib/PortableApps/src/remote_battery.c',
            'lib/PortableApps/include/PortableQuickActions.h','lib/PortableApps/include/PortableRemoteBattery.h',
        }
        self.assertTrue(required.issubset(receipt['source_sha256']))
        self.assertEqual(receipt['source_sha256'],{
            name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest()
            for name in shared.QUICK_REMOTE_BATTERY_SOURCES})
        sdk_hash=hashlib.sha256((self.includes/shared.QUICK_REMOTE_BATTERY_SDK).read_bytes()).hexdigest()
        self.assertEqual(receipt['sdk_sha256'],{shared.QUICK_REMOTE_BATTERY_SDK:sdk_hash})
        self.assertEqual(sdk_hash,shared.QUICK_REMOTE_BATTERY_SDK_SHA256)

    def test_receipt_documents_bounded_local_implementation_not_policy(self):
        *_,record=self.configure(*HOST_OPTIONS,*SELECT)
        receipt=record['quick_remote_battery']
        self.assertTrue(receipt['selected'])
        self.assertEqual(receipt['status'],'local-development-not-reserved-or-published')
        self.assertEqual(receipt['capability'],'bluetooth.sensors@1')
        self.assertEqual(receipt['instance'],0)
        self.assertEqual(receipt['declaration'],'optional')
        self.assertEqual(receipt['grant_authority'],'explicit-owner-policy-only')
        self.assertEqual(receipt['additional_declaration_rows'],1)
        self.assertEqual(receipt['additional_grant_rows_if_authorized'],1)
        self.assertEqual(receipt['selection'],'manual-address-and-type-per-session')
        self.assertEqual(receipt['scan'],'manual-bounded-passive')
        self.assertEqual(receipt['scan_duration_ms'],15000)
        self.assertEqual(receipt['freshness_ms'],10000)
        self.assertIn('#define PQB_FRESH_MS 10000u',(ROOT/'lib/PortableApps/include/PortableRemoteBattery.h').read_text())
        for field in ('default_selected','auto_target_selection','persistent_selection',
                      'background_scan','rf_policy_changed','hardware_verified','publication_ready'):
            self.assertIs(receipt[field],False)
        self.assertFalse(receipt['version_verified_unused'])

    def test_local_identity_does_not_change_ordinary_or_client_versions(self):
        normal,_=self.arguments(*HOST_OPTIONS)
        remote,_=self.arguments(*HOST_OPTIONS,*SELECT)
        self.assertEqual(shared.version(normal,'paper_clock','0.0.1'),'0.3.20')
        normal.crash_report_sd=True
        self.assertEqual(shared.version(normal,'paper_clock','0.0.1'),'0.4.6')
        self.assertEqual(shared.version(remote,'paper_clock','0.0.1'),'0.3.20')
        self.assertEqual(shared.version(remote,'settings','1.0.0'),'1.0.0')
        client,_=self.arguments('--resident-shell-client','--alarm-client')
        self.assertEqual(shared.version(client,'settings','1.0.0'),'1.3.28')
        self.assertEqual(self.configure(*HOST_OPTIONS,*SELECT)[4]['quick_remote_battery']['local_app_version'],'0.4.21')

    def test_final_paper_version_and_grant_count_are_opt_in(self):
        # Execute the real version-selection tail, not a duplicate of it. Earlier
        # generic build/version helpers have already selected an arbitrary base.
        tree=ast.parse((ROOT/'scripts/build_paper_clock.py').read_text())
        build=next(node for node in tree.body if isinstance(node,ast.FunctionDef) and node.name=='build')
        start=next(i for i,node in enumerate(build.body) if isinstance(node,ast.If)
                   and ast.unparse(node.test)=='a.boot_animation'
                   and any(isinstance(child,ast.Assign) and ast.unparse(child.targets[0])=='version' for child in node.body))
        end=next(i for i in range(start,len(build.body)) if isinstance(build.body[i],ast.Assign)
                 and ast.unparse(build.body[i].targets[0])=='needs')
        tail=compile(ast.Module(body=build.body[start:end],type_ignores=[]),'<paper version selection>','exec')
        for selected in (False,True):
            a=argparse.Namespace(boot_animation=True,installed_catalog_volume_instance=9,
                                 deferred_foreground_services=True,sparse_start=True,
                                 automatic_provision=True,quick_remote_battery=selected,quick_remote_battery_version='0.4.21')
            values={'a':a,'p':argparse.ArgumentParser(),'version':'0.0.1','portable_quick_build':shared}
            exec(tail,values)
            self.assertEqual(values['version'],'0.4.21' if selected else '0.4.20')
        grant_check=next(node for node in ast.walk(build) if isinstance(node,ast.If)
                         and any(isinstance(child,ast.Raise) and 'Desk clock grant count mismatch' in ast.unparse(child) for child in node.body))
        count=compile(ast.Expression(body=grant_check.test.comparators[0]),'<paper grant count>','eval')
        for sparse in (False,True):
            for selected in (False,True):
                a=argparse.Namespace(sparse_start=sparse,ble_broadcast=sparse,contexts_rf_only=sparse,
                                     crash_report_sd=True,experimental_atomic_quick=True,
                                     experimental_settings_availability=True,quick_remote_battery=selected)
                self.assertEqual(eval(count,{'a':a}),20 if sparse else 15)

    def test_crash_profile_mandatory_row_counts_stay_unchanged(self):
        tree=ast.parse((ROOT/'scripts/portable_quick_build.py').read_text())
        receipt=next(node for node in ast.walk(tree) if isinstance(node,ast.Dict)
                     and any(isinstance(key,ast.Constant) and key.value=='native_requirement_rows' for key in node.keys))
        fields={key.value:value for key,value in zip(receipt.keys,receipt.values) if isinstance(key,ast.Constant)}
        for field,base in (('native_requirement_rows',17),('native_policy_rows',18)):
            expression=compile(ast.Expression(body=fields[field]),'<crash profile rows>','eval')
            self.assertEqual(eval(expression,{'remote':False}),base)
            self.assertEqual(eval(expression,{'remote':True}),base)

    def test_emitted_manifest_uses_optional_only_when_selected(self):
        tree=ast.parse((ROOT/'scripts/build_paper_clock.py').read_text())
        build=next(node for node in tree.body if isinstance(node,ast.FunctionDef) and node.name=='build')
        start=next(i for i,node in enumerate(build.body) if isinstance(node,ast.Assign)
                   and ast.unparse(node.targets[0])=='manifest')
        end=next(i for i in range(start,len(build.body)) if isinstance(build.body[i],ast.Expr)
                 and 'write_text' in ast.unparse(build.body[i]))
        manifest_code=compile(ast.Module(body=build.body[start:end],type_ignores=[]),'<paper manifest emission>','exec')
        for selected in (False,True):
            values={'a':argparse.Namespace(quick_remote_battery=selected),'version':'0.4.7',
                    'needs':[{'capability':'display.output','api':1}],'portable_quick_build':shared}
            exec(manifest_code,values)
            manifest=values['manifest']
            self.assertEqual(manifest['requires'],values['needs'])
            self.assertNotIn(CAPABILITY,manifest['requires'])
            if selected:self.assertEqual(manifest['optional'],[CAPABILITY])
            else:
                self.assertNotIn('optional',manifest)
                original={'type':'application','id':'paper_clock','version':values['version'],
                          'architecture':'xtensa-esp32s3','file_name':'default.elf',
                          'entry':'app_main','requires':values['needs']}
                self.assertEqual(json.dumps(manifest,indent=2)+'\n',json.dumps(original,indent=2)+'\n')


if __name__=='__main__':unittest.main()
