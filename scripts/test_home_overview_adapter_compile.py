#!/usr/bin/env python3
"""Replay the owning overview adapter command; reproduce both fixed regressions.
Uses the real staged SDK/compiler/feature flags from a successful view receipt.
Only temporary output and negative-control source copies are modified.
"""
import argparse,hashlib,json,pathlib,shutil,subprocess,tempfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def require(ok,message):
 if not ok:raise ValueError(message)
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--view-receipt',type=pathlib.Path,required=True)
p.add_argument('--output',type=pathlib.Path,required=True)
a=p.parse_args()
receipt=json.loads(a.view_receipt.read_bytes())
source=ROOT/'lib/PortableApps/src/adapter.c'
require(receipt['prefix']=='home_overview_','Not the owning overview receipt')
rows=[row for row in receipt['commands'] if '-c' in row and str(source) in row]
require(len(rows)==1,'Exactly one real adapter compilation required')
command=rows[0]
for flag in ('-Wall','-Wextra','-Werror','-DPORTABLE_SHARED_HOME_CANVAS',
             '-DPORTABLE_QUICK_RADIOS','-DPORTABLE_DESK_CLOCK_SPARSE_START',
             '-DPORTABLE_QUICK_ATOMIC_PREFERENCES','-DALARM_SERVICE_TAGGED_V2'):
 require(flag in command,'Missing actual feature/warning flag: '+flag)
require(not any(x.startswith('-Wno-') for x in command),'Warning suppressions are not this regression')
tracked={pathlib.Path(path):digest for path,digest in receipt['compiled_headers_sha256'].items()}
tracked[source]=receipt['sources'][str(source)]
for path,digest in tracked.items():require(sha(path)==digest,'Compiled input changed: '+str(path))
a.output.mkdir(parents=True,exist_ok=False)
results=[]
with tempfile.TemporaryDirectory(prefix='home-overview-compile-') as tmp:
 root=pathlib.Path(tmp)
 for name in ('current','old-alarm-scope','old-radio-wrapper'):
  selected=source
  if name!='current':
   copy=root/name
   shutil.copytree(ROOT/'lib/PortableApps',copy/'lib/PortableApps')
   selected=copy/'lib/PortableApps/src/adapter.c'
   if name=='old-alarm-scope':
    path=selected.parent/'resident_shell.inc'
    text=path.read_text()
    require(' const alarm_service_v1 *alarm=alarm_sleep_api();\n' in text,'Alarm fix seam changed')
    text=text.replace(' const alarm_service_v1 *alarm=alarm_sleep_api();\n','')
    text=text.replace('&&alarm!=NULL','&&alarms.api!=NULL').replace('alarm_service_descriptor(alarm)','alarm_service_descriptor(alarms.api)')
    path.write_text(text)
   else:
    path=selected.parent/'sparse_clock_adapter.inc';text=path.read_text()
    before='#ifndef PORTABLE_SHARED_HOME_CANVAS\n/* Shared Home drains Wi-Fi and Bluetooth in PortableHomeSleep before holds. */\n'
    after='#endif\nstatic bool desk_radios_resume'
    require(before in text and after in text,'Radio fix seam changed')
    path.write_text(text.replace(before,'').replace(after,'static bool desk_radios_resume'))
    text=selected.read_text()
    for line in ('static bool desk_radios_suspend(const risc_runtime_api_v1 *);',
                 '#define pqa_radios_suspend desk_radios_suspend'):
     block='#ifndef PORTABLE_SHARED_HOME_CANVAS\n'+line+'\n#endif'
     require(block in text,'Radio declaration/macro seam changed')
     text=text.replace(block,line)
    selected.write_text(text)
  argv=list(command)
  argv[argv.index(str(source))]=str(selected)
  argv[argv.index('-o')+1]=str(root/(name+'.o'))
  argv[argv.index('-MF')+1]=str(root/(name+'.d'))
  run=subprocess.run(argv,text=True,capture_output=True)
  (a.output/(name+'.log')).write_text(run.stdout+run.stderr)
  if name=='current':require(run.returncode==0,run.stderr)
  elif name=='old-alarm-scope':
   require(run.returncode!=0 and 'alarms' in run.stderr and 'undeclared' in run.stderr,run.stderr)
  else:
   require(run.returncode!=0 and 'desk_radios_suspend' in run.stderr and 'unused-function' in run.stderr,run.stderr)
  results.append({'case':name,'command':argv,'returncode':run.returncode,
                  'log_sha256':sha(a.output/(name+'.log')),
                  'object_sha256':sha(root/(name+'.o')) if run.returncode==0 else None})
for path,digest in tracked.items():require(sha(path)==digest,'Input mutated: '+str(path))
(a.output/'receipt.json').write_text(json.dumps({'schema':1,'view_receipt_sha256':sha(a.view_receipt),
 'cases':results,'compiler_sha256':sha(pathlib.Path(command[0])),
 'compiled_inputs_sha256':{str(path):digest for path,digest in tracked.items()},
 'scope':'Actual target adapter compilation plus isolated historical negative controls; no target execution.'},indent=2)+'\n')
print('Actual overview strict compile and two historical negative controls PASS')
