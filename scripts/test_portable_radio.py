#!/usr/bin/env python3
"""Exercise real radio lifecycle hooks with the established failure fixtures."""
import os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];out=ROOT/'build/portable-radio';out.mkdir(parents=True,exist_ok=True)
# The exact same ownership failure matrix is applied to the independent radio
# hooks; this does not compile the audio feature or alias its production API.
fixture=(ROOT/'test/native_apps/portable_alarm_test.c').read_text().replace('PORTABLE_AUDIO_SESSION','PORTABLE_RADIO_SESSION').replace('portable_audio_','portable_radio_')
(out/'radio_alarm_fixture.c').write_text(fixture)
quick=(ROOT/'test/native_apps/quick_adapter_test.c').read_text().replace('"portable_alarm_test.c"','"radio_alarm_fixture.c"').replace('(void)c;ble_state=on?1:0;','(void)c;assert(!application_audio);ble_state=on?1:0;')
quick=quick.replace('capture_directory=argv[2];setup();','capture_directory=argv[2];application_audio=false;setup();application_audio=true;')
(out/'radio_quick_fixture.c').write_text(quick)
for san in (False,True):
 flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_NOVA_UI','-DPORTABLE_RADIO_SESSION','-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include'),'-I'+str(ROOT/'test/native_apps')]
 if san:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0')
 exe=out/f'alarm-{int(san)}'
 subprocess.run([os.environ.get('CC','cc'),*flags,str(ROOT/'Apps/settings.c'),str(out/'radio_alarm_fixture.c'),'-o',str(exe)],check=True)
 for case in range(16):subprocess.run([str(exe),str(case)],check=True,env=env,timeout=20)
 exe=out/f'quick-{int(san)}'
 sources=[ROOT/'Apps/settings.c',out/'radio_quick_fixture.c']+[ROOT/'lib/PortableApps/src'/name for name in ['quick_actions.c','quick_render.c','quick_session.c','quick_radios.c']]
 subprocess.run([os.environ.get('CC','cc'),*flags,'-DPORTABLE_QUICK_RADIOS',*map(str,sources),'-o',str(exe)],check=True)
 for case in range(13):subprocess.run([str(exe),str(case)],check=True,env=env,timeout=20)
print('Radio alarm, cue, retained failure, sleep, handoff and quick-controls lifecycle: 58 executions passed')
