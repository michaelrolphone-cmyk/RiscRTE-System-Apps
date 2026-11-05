#!/usr/bin/env python3
"""Pinned deployed Wi-Fi 1.1.28 feature profile plus opted-in modal policy."""
import argparse,sys,runpy
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--runtime',type=Path,required=True);p.add_argument('--utilities',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
s=Path(__file__).resolve().parents[1]
sys.argv=[str(s/'scripts/build_portable_wifi.py'),'--shared-text-policy','--experimental-settings-availability','--experimental-atomic-wifi','--time-profile','x4-native-time','--native-time-runtime-repo',str(a.runtime.resolve()),'--tagged-alarm-utilities',str(a.utilities.resolve()),'--resident-shell-client','--resident-runtime-sdk',str((a.runtime/'sdk/app').resolve()),'--resident-policy','--alarm-client','--wifi-instance','15','--return-app','springboard.elf','--home-app','default.elf','--raster-snapshot','--stage-logs','--touch-scrolling','--ble-broadcast','--navigation','--output-dir',str(a.output.resolve())]
runpy.run_path(sys.argv[0],run_name='__main__')
