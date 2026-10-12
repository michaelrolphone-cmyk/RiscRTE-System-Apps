#!/usr/bin/env python3
"""Actual automatic Home binding, policy encoders and claimed-preset flow."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--target',type=Path,required=True);p.add_argument('--sanitize',action='store_true');p.add_argument('--output',type=Path,default=r/'build/home-automatic');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
inc=a.output/'include';receipt=json.loads((a.target/'build-evidence.json').read_text());shutil.copytree(receipt['paper_transition']['compiled_include_directory'],inc,dirs_exist_ok=True)
for header in (r/'lib/PortableApps/include').glob('*.h'):
 if not header.name.startswith(('Risc','AlarmService')):shutil.copyfile(header,inc/header.name)
cases='source-conflict conflict preset-no-callback editor-origin scope same-controls rearm-failed edge-readback stale-after-edge cancel-busy normal off same-timers invalid-sample partial uncertain edge-failed missing wrong-namespace state-busy read-io stale submit-io busy dirty-submit collect-io bad-receipt release-failed dirty-acquire cancel cancel-race cancel-error preset preset-partial preset-low preset-mask preset-alert rules-order rules-partial rules-low rules-notify'.split()
flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+str(inc)]
if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
exe=a.output/'home-automatic';cmd=[os.environ.get('CC','cc'),*flags,str(r/'test/native_apps/home_automatic_test.c'),'-o',str(exe)];subprocess.run(cmd,check=True)
results=[]
for case in cases:
 s=subprocess.run([str(exe),case],check=True,capture_output=True,text=True,timeout=10,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')).stdout.strip();print(s);results.append({'case':case,'result':s})
rules=a.output/'home-automatic-rules';rule_cmd=[os.environ.get('CC','cc'),*flags,'-I'+str(r/'lib/NativeApps/include'),str(r/'test/native_apps/home_automatic_rules_test.c'),'-o',str(rules)];subprocess.run(rule_cmd,check=True)
for case in 'rule-loop rule-loop-partial rule-loop-low rule-loop-message rule-loop-launch rule-loop-timer'.split():
 s=subprocess.run([str(rules),case],check=True,capture_output=True,text=True,timeout=10,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')).stdout.strip();print(s);results.append({'case':case,'result':s})
files=['test/native_apps/home_automatic_rules_test.c','lib/PortableApps/src/contexts_rules_adapter.inc','lib/PortableApps/include/PortablePreferenceAutomatic.h','lib/PortableApps/src/home_automatic.inc','lib/PortableApps/include/PortableContextsClient.h','test/native_apps/home_automatic_test.c','scripts/test_home_automatic.py']
(a.output/'receipt.json').write_text(json.dumps({'sanitized':a.sanitize,'provider_double':True,'hardware_tested':False,'compile_command':cmd,'cases':results,'sha256':{f:hashlib.sha256((r/f).read_bytes()).hexdigest() for f in files}},indent=2)+'\n')
