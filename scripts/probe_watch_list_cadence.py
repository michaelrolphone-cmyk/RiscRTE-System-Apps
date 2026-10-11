#!/usr/bin/env python3
"""Measure real Watch Settings timing under modeled display latency."""
import json,os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/watch-list-cadence';out.mkdir(parents=True,exist_ok=True)
binary=out/'probe';command=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DPORTABLE_NOVA_UI','-DPORTABLE_TOUCH_ROTATION=0','-I'+str(root/'lib/PortableApps/include'),'-I'+str(root/'lib/NativeApps/include'),str(root/'Apps/settings.c'),str(root/'test/native_apps/watch_list_cadence_probe.c'),'-o',str(binary)]
subprocess.run(command,check=True);rows=[]
for latency in (0,16,40,80,240):
 rows.append(json.loads(subprocess.check_output([str(binary),str(latency)],text=True)))
rows.append(json.loads(subprocess.check_output([str(binary),'240','80'],text=True)))
proof={'source':subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip(),'scope':'Real Settings/controller/adapter; synthetic wall-clock input and display latency; no target hardware measurements','observations':rows}
(out/'cadence.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(proof,indent=2))
