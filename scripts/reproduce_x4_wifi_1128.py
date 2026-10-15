#!/usr/bin/env python3
"""Rebuild the recovered deployed Wi-Fi feature profile without compacting it."""
import argparse,sys,runpy,hashlib,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--system',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True);p.add_argument('--utilities',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
s=a.system.resolve();sys.path.insert(0,str(s/'scripts'));import portable_quick_build
old=portable_quick_build.version
portable_quick_build.version=lambda args,name,v:'1.1.28' if name=='wifi_settings' else old(args,name,v)
sys.argv=[str(s/'scripts/build_portable_wifi.py'),'--experimental-settings-availability','--experimental-atomic-wifi','--time-profile','x4-native-time','--native-time-runtime-repo',str(a.runtime.resolve()),'--tagged-alarm-utilities',str(a.utilities.resolve()),'--resident-shell-client','--resident-runtime-sdk',str((a.runtime/'sdk/app').resolve()),'--resident-policy','--alarm-client','--wifi-instance','15','--return-app','springboard.elf','--home-app','default.elf','--raster-snapshot','--stage-logs','--touch-scrolling','--ble-broadcast','--navigation','--output-dir',str(a.output.resolve())]
runpy.run_path(sys.argv[0],run_name='__main__')
digest=hashlib.sha256((a.output/'wifi_settings.elf').read_bytes()).hexdigest()
assert digest=='31ded00c9c1326b24a850294b78114be1ac7b7f1e59bd2c2d6a348fa840bfb4d',digest
print('Recovered deployed Wi-Fi executable sections verified; four compiler-generated .strtab names differ.')
