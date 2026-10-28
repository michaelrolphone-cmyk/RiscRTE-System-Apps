#!/usr/bin/env python3
"""Qualify generic Settings fallback builder inputs and actual compact controller."""
import contextlib
import io
import os
from pathlib import Path
import subprocess
import tempfile
from build_portable_settings import argument_parser
import settings_sleep_defaults as defaults

ROOT=Path(__file__).resolve().parents[1]
p=argument_parser()
assert defaults.configure(p.parse_args([]),p)==[]
for profile in ('default','native-time-touch'):
 for mode in ('deep','light','hybrid'):
  for seconds in (5,15,60,3600):
   a=p.parse_args(['--settings-profile',profile,'--sleep-settings','--idle-timer-settings',
      '--default-sleep-mode',mode,'--default-idle-seconds',str(seconds)])
   assert defaults.configure(a,p)==['-DPORTABLE_SETTINGS_DEFAULT_SLEEP_MODE=PORTABLE_SLEEP_'+mode.upper(),
       '-DPORTABLE_SETTINGS_IDLE_TIMER',f'-DPORTABLE_SETTINGS_DEFAULT_IDLE_SECONDS={seconds}u']
   assert defaults.record(a)['mode']==mode and defaults.record(a)['idle_seconds']==seconds
   assert defaults.record(a)['idle_timer_editable'] and not defaults.record(a)['deep_timer_editable']
for profile in ('default','native-time-touch'):
 for mode in ('deep','light','hybrid'):
  for seconds in (5,15,60,3600):
   a=p.parse_args(['--settings-profile',profile,'--sleep-settings','--sleep-timer-settings',
      '--default-sleep-mode',mode,'--default-idle-seconds',str(seconds)])
   assert defaults.configure(a,p)==['-DPORTABLE_SETTINGS_DEFAULT_SLEEP_MODE=PORTABLE_SLEEP_'+mode.upper(),
       '-DPORTABLE_SETTINGS_BOTH_TIMERS',f'-DPORTABLE_SETTINGS_DEFAULT_IDLE_SECONDS={seconds}u']
   assert defaults.record(a)['idle_timer_editable'] and defaults.record(a)['deep_timer_editable']
invalid=[['--default-sleep-mode','deep'],['--idle-timer-settings'],['--default-idle-seconds','15'],
         ['--sleep-settings','--default-sleep-mode','wrong']]
invalid += [['--sleep-timer-settings'],['--sleep-settings','--idle-timer-settings','--sleep-timer-settings']]
invalid += [['--settings-profile',profile,'--sleep-settings','--sleep-timer-settings'] for profile in ('x4-desk-clock','x4-native-time')]
invalid += [['--sleep-settings','--idle-timer-settings','--default-idle-seconds',value] for value in ('0','4','3601','1.2','nan')]
invalid += [['--settings-profile',profile,'--sleep-settings','--default-sleep-mode','deep'] for profile in ('x4-desk-clock','x4-native-time')]
for args in invalid:
 with contextlib.redirect_stderr(io.StringIO()):
  try:defaults.configure(p.parse_args(args),p)
  except SystemExit:pass
  else:raise AssertionError(args)
with tempfile.TemporaryDirectory(prefix='settings-sleep-defaults-') as temporary:
 binary=Path(temporary)/'test'
 for both in (False,True):
  for sanitize in (False,True):
   for mode,seconds in ((None,None),('DEEP',15),('LIGHT',5),('HYBRID',3600)):
    flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
           '-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/NativeApps/include')]
    if both:flags += ['-DPORTABLE_SETTINGS_BOTH_TIMERS']
    if mode:flags += ['-DPORTABLE_SETTINGS_DEFAULT_SLEEP_MODE=PORTABLE_SLEEP_'+mode,f'-DPORTABLE_SETTINGS_DEFAULT_IDLE_SECONDS={seconds}u']
    if sanitize:flags += ['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
    subprocess.run([os.environ.get('CC','cc'),*flags,str(ROOT/'Apps/settings.c'),
        str(ROOT/'test/native_apps/settings_sleep_defaults_test.c'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
print('Settings fallback CLI matrix and 16 actual-controller normal/ASan/UBSan builds PASS')
