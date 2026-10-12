#!/usr/bin/env python3
"""Opt-in atomic Wi-Fi live-controller tests; no radio, SD or production broker."""
import argparse,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--sanitize',action='store_true');p.add_argument('--output',type=Path,default=Path('/tmp/system-wifi-atomic-tests'));a=p.parse_args()
root=Path(__file__).resolve().parents[1];a.output.mkdir(parents=True,exist_ok=True)
flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g','-no-pie'] if a.sanitize else []
binary=a.output/'wifi_atomic_workflow'
subprocess.run([os.environ.get('CC','cc'),'-std=c11','-DTEST_IDLE_ELIGIBILITY','-Wall','-Wextra','-Werror',*flags,*['-I'+str(root/x) for x in ['lib/PortableApps/include','lib/NativeApps/include','lib/RemoteFiles/vendor/tinydtls_sha2']],str(root/'test/native_apps/wifi_atomic_workflow_test.c'),str(root/'lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c'),'-o',str(binary)],check=True)
for case in range(19):subprocess.run([str(binary),str(case)],check=True)
print('Atomic capability is opt-in; native publication and product lifecycle require separate Runtime qualification.')
