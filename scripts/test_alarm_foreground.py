#!/usr/bin/env python3
"""Build or host-test the shared foreground alarm controller and real scene."""
import argparse, os, shutil, subprocess
from pathlib import Path
from build_wifi_manager import sdk
ROOT=Path(__file__).resolve().parents[1]
def stage(runtime, output):
    inc=sdk(runtime,output)
    for name in ('RiscAlarmForegroundV1.h','RiscSceneModalV1.h'):
        shutil.copyfile(ROOT/'sdk/app'/name,inc/name)
    shutil.copyfile(ROOT/'Services/alarm_foreground/AlarmServiceV2.h',inc/'AlarmServiceV2.h')
    return inc
def run(runtime,output,sanitize=False):
    output.mkdir(parents=True,exist_ok=True);inc=stage(runtime,output/'sdk')
    cc=os.environ.get('CC','cc')
    flags=['-std=c11','-g','-O1','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+str(inc)]
    if sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
    obj=output/'alarm.o';exe=output/'test'
    subprocess.run([cc,*flags,'-Dt5_driver_get=alarm_driver_get','-c',str(ROOT/'Services/alarm_foreground/host.c'),'-o',str(obj)],check=True)
    subprocess.run([cc,*flags,str(ROOT/'Services/scene_host/host.c'),str(ROOT/'test/alarm_foreground/host_test.c'),str(obj),'-o',str(exe)],check=True)
    for mode in ('alert','pending','cue','loss','output'):
        subprocess.run([str(exe),mode],check=True,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
    print('Shared alarm controller with real modal scene PASS; installed products not qualified')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--runtime',required=True,type=Path);p.add_argument('--output',required=True,type=Path);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
    run(a.runtime.resolve(),a.output.resolve(),a.sanitize)
