#!/usr/bin/env python3
"""Test selected unpadded Settings hours through real UI and RTC paths."""
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
out=root/'build/hour-boundaries';out.mkdir(parents=True,exist_ok=True)
count=0
for selected in (False,True):
 for sanitized in (False,True):
  for profile,defines in [('raw',[]),('denver',['-DPORTABLE_RTC_UTC8_DENVER','-DPORTABLE_TOUCH_ROTATION=180']),('navigation',['-DPORTABLE_INPUT_NAVIGATION']),('nova',['-DPORTABLE_NOVA_UI'])]:
   binary=out/(profile+'-'+str(int(selected))+'-'+str(int(sanitized)))
   flags=['-DPORTABLE_UNPADDED_HOURS'] if selected else []
   if sanitized:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all','-no-pie']
   subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*defines,
       '-I'+str(root/'lib/PortableApps/include'),'-I'+str(root/'lib/NativeApps/include'),str(root/'Apps/settings.c'),
       str(root/'test/native_apps/portable_time_format_test.c'),'-o',str(binary)],check=True)
   for case in range(19):
    if case in (13,14) and profile!='navigation':continue
    subprocess.run([str(binary),str(case)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});count+=1
print(f'{count} actual Settings legacy/selected raw/Denver/navigation/Nova scenarios passed normal and ASan/UBSan')
