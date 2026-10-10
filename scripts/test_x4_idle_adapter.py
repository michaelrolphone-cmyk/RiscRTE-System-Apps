#!/usr/bin/env python3
"""Actual native Settings controller/adapter across returning automatic idle."""
import argparse,os,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--candidate',type=Path,required=True);a=p.parse_args()
out=r/'build/idle-policy/adapter-tests';out.mkdir(parents=True,exist_ok=True)
names='PortableSetTime.c PortableRealtimeClient.c PortableTimeZone.c PortableTimeZoneCatalog.c PortableTimeZonePreference.c quick_actions.c quick_session.c quick_render.c quick_radios.c'.split()
for san in (False,True):
 flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DTEST_X4_IDLE_SETTINGS','-DTEST_NATIVE_SETTINGS_QUICK','-DALARM_SERVICE_TAGGED_V2','-DPORTABLE_X4_IDLE_POLICY','-DPORTABLE_LOW_BATTERY','-DPORTABLE_APP_SLEEP_LOCAL','-DPORTABLE_PAPER_TRANSITIONS','-DPORTABLE_RADIO_CONTINUOUS_CAPTURE','-DPORTABLE_AUDIO_CONTINUOUS_CAPTURE']
 if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 exe=out/('sanitized' if san else 'normal')
 subprocess.run([os.environ.get('CC','cc'),*flags,'-I'+str(a.candidate/'idle-sdk/include'),'-I'+str(r/'lib/NativeApps/include'),str(r/'Apps/settings_native_entry.c'),str(r/'test/native_apps/x4_idle_settings_test.c'),*[str(r/'lib/PortableApps/src'/n) for n in names],'-Wl,--wrap=free','-o',str(exe)],check=True)
 for result in (0,1,-2):subprocess.run([exe,str(result)],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'),check=True,timeout=20)
