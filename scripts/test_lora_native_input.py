#!/usr/bin/env python3
"""Production private LoRa ownership reducer, normal and ASan/UBSan."""
import os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
out=root/'build/lora-native-input';out.mkdir(parents=True,exist_ok=True)
for sanitize in (False,True):
    exe=out/('sanitized' if sanitize else 'normal')
    flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitize else []
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-I'+str(root/'lib/PortableApps/include'),'-I'+str(root/'lib/PortableApps/src'),str(root/'test/native_apps/lora_native_input_test.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})

# Private hooks must not expand the dynamic ABI.
obj=out/'adapter.o'
subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-DPORTABLE_LORA_NATIVE_UI','-DPORTABLE_NOVA_UI','-DPORTABLE_NATIVE_CANVAS','-I'+str(root/'lib/PortableApps/include'),'-I'+str(root/'lib/NativeApps/include'),'-c',str(root/'lib/PortableApps/src/adapter.c'),'-o',str(obj)],check=True)
symbols=subprocess.check_output(['readelf','--wide','--symbols',str(obj)],text=True)
for name in ('begin','layout','present','ready','tap_origin'):
 rows=[line for line in symbols.splitlines() if line.split()[-1:]==['portable_lora_native_'+name]]
 assert len(rows)==1 and 'GLOBAL HIDDEN' in rows[0],rows
print('PASS: five private LoRa hooks retain hidden visibility')
