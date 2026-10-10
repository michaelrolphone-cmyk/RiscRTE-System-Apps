#!/usr/bin/env python3
"""Compile and exercise the real optional presenter, without hardware."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
from scene_sdk import stage_sdk
from build_scene_services import PROFILE_FLAGS

ROOT = Path(__file__).resolve().parents[1]
MODES = ('behavior', 'inflight', 'stale-frame', 'gap', 'transient', 'held',
         'busy-acquire', 'status-fault', 'snapshot-fault', 'provider-fault', 'close-fault',
         'native-open-before', 'native-poll-before', 'native-close-before', 'native-open-get_info', 'native-open-subscribe', 'native-open-snapshot',
         'native-open-snapshot-fail', 'native-open-foreground', 'native-open-reset',
         'native-poll-poll', 'native-poll-next', 'native-poll-snapshot',
         'native-poll-nav_poll', 'native-poll-acquire', 'native-poll-submit',
         'native-poll-status', 'native-close-status', 'native-close-unsubscribe',
         'native-close-foreground', 'native-close-reset')
KEYBOARD_MODES = ('keyboard-touch', 'keyboard-navigation', 'keyboard-pending',
                  'keyboard-stale', 'keyboard-hardware', 'keyboard-invalid', 'keyboard-gesture',
                  'keyboard-raster', 'keyboard-superseded', 'keyboard-native-snapshot', 'keyboard-native-reset')


def run(runtime: Path, output: Path, sanitize: bool) -> int:
    include = stage_sdk(runtime, ROOT, output / 'sdk')
    compiler = os.environ.get('CC', 'cc')
    flags = ['-std=c11', '-O1', '-Wall', '-Wextra', '-Werror']
    if sanitize:
        flags += ['-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                  '-fno-omit-frame-pointer']
    exe = output / 'scene-host-test'
    subprocess.run([compiler, *flags, '-I'+str(include),
                    str(ROOT/'Services/scene_host/host.c'),
                    str(ROOT/'test/scene/host_test.c'), '-o', str(exe)], check=True)
    env = dict(os.environ)
    env.setdefault('ASAN_OPTIONS', 'detect_leaks=0')
    for profile in ('watch', 'paper', 'gray'):
        for mode in MODES:
            command = [str(exe), mode, profile]
            if mode == 'behavior':
                command += [str(output/f'{profile}.frame')]
            subprocess.run(command, env=env, check=True)
    keyboard_exe = output / 'scene-keyboard-test'
    subprocess.run([compiler, *flags, '-I'+str(include),
                    str(ROOT/'Services/scene_host/host.c'),
                    str(ROOT/'test/scene/keyboard_test.c'), '-o', str(keyboard_exe)], check=True)
    for profile in ('watch', 'paper', 'gray'):
        for mode in KEYBOARD_MODES:
            command = [str(keyboard_exe), mode, profile]
            if mode == 'keyboard-raster':
                command += [str(output/f'{profile}-keyboard.frame')]
            subprocess.run(command, env=env, check=True)
    # Compile the actual independently packaged profiles with the exact target
    # builder flags, then assert against the installed panel/touch contract.
    for name, profile_flags in PROFILE_FLAGS.items():
        obj=output/(name+'.o');profile_exe=output/(name+'-orientation')
        subprocess.run([compiler,*flags,'-I'+str(include),*profile_flags,
                        '-Dt5_driver_get=scene_test_profile_get','-c',
                        str(ROOT/'Services/scene_profile/profile.c'),'-o',str(obj)],check=True)
        subprocess.run([compiler,*flags,'-DTEST_PACKAGED_PROFILE','-I'+str(include),
                        str(ROOT/'Services/scene_host/host.c'),str(ROOT/'test/scene/keyboard_test.c'),
                        str(obj),'-o',str(profile_exe)],check=True)
        subprocess.run([str(profile_exe),'profile-orientation','paper' if name=='portrait-monochrome' else 'watch'],env=env,check=True)
    # Regression witness: the previously packaged90 policy must fail the
    # independent raster-position check, not merely a numeric rotation check.
    bad=output/'old-portrait.o';bad_exe=output/'old-portrait-orientation'
    subprocess.run([compiler,*flags,'-I'+str(include),'-DSCENE_PROFILE_PAPER=1',
                    '-DSCENE_DISPLAY_ROTATION=90','-Dt5_driver_get=scene_test_profile_get',
                    '-c',str(ROOT/'Services/scene_profile/profile.c'),'-o',str(bad)],check=True)
    subprocess.run([compiler,*flags,'-DTEST_PACKAGED_PROFILE','-I'+str(include),
                    str(ROOT/'Services/scene_host/host.c'),str(ROOT/'test/scene/keyboard_test.c'),
                    str(bad),'-o',str(bad_exe)],check=True)
    rejected=subprocess.run([str(bad_exe),'profile-orientation','paper'],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    assert rejected.returncode and 'installed_pixel_level' in rejected.stderr,rejected.stderr
    print('Old portrait90 rejected by independent installed physical raster assertion PASS')
    return (len(MODES)+len(KEYBOARD_MODES))*3+len(PROFILE_FLAGS)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='risc-scene-host-') as temporary:
        count = run(args.runtime.resolve(), Path(temporary), args.sanitize)
    print(f'{count} presenter/profile executions passed; hardware remains untested.')
