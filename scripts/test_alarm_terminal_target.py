#!/usr/bin/env python3
"""Preserve both API1/API2 opt-out Xtensa bytes; compile selected guard profiles."""
import argparse,hashlib,json,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--baseline',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--utilities',type=Path,required=True);p.add_argument('--xtensa-cc',type=Path,required=True)
a=p.parse_args();out=ROOT/'build/alarm-terminal-target';out.mkdir(parents=True,exist_ok=True);evidence={}
flags=['-std=c11','-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-Wall','-Wextra','-Werror','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_NATIVE_TIME_TOOLBAR','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_STAGE_LOGS']
with tempfile.TemporaryDirectory() as tmp:
 for tagged in (False,True):
  for quick in (False,True):
   profile=flags+(['-DALARM_SERVICE_TAGGED_V2'] if tagged else [])+(['-DPORTABLE_QUICK_ACTIONS'] if quick else [])
   objects=[];label=f'{int(tagged)}-{int(quick)}'
   for side,root in [('before',a.baseline),('after',ROOT)]:
    stage=Path(tmp)/(side+label);inc=stage/'include';shutil.copytree(root/'lib/PortableApps/include',inc);shutil.copytree(root/'lib/PortableApps/time',stage/'time')
    for name in ('RiscRuntimeV1.h','RiscRealtimeV1.h'):shutil.copyfile(a.runtime/'sdk/app'/name,inc/name)
    includes=['-I'+str(inc),'-I'+str(a.utilities/'lib/Alarm/include'),'-I'+str(root/'lib/NativeApps/include')]
    obj=out/f'{label}-{side}.o';objects.append(obj)
    command=[a.xtensa_cc,*profile,*includes,root/'lib/PortableApps/src/adapter.c','-c','-o',obj];subprocess.run(list(map(str,command)),check=True)
    if side=='after':subprocess.run(list(map(str,[a.xtensa_cc,*profile,'-DPORTABLE_ALARM_TERMINAL_RETENTION',*includes,root/'lib/PortableApps/src/adapter.c','-c','-o',out/f'{label}-selected.o'])),check=True)
   assert objects[0].read_bytes()==objects[1].read_bytes(),label
   evidence[label]=hashlib.sha256(objects[1].read_bytes()).hexdigest()
(out/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
print('API1/API2, plain/Quick: four flag-off Xtensa objects byte-identical; four selected profiles compile PASS')
