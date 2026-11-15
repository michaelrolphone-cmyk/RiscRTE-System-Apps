#!/usr/bin/env python3
"""Flash one exact USB MAC, then provision from private local configuration.
Credentials and per-board OTA keys are never printed or embedded in the image.
"""
import argparse
import json
from pathlib import Path
import subprocess
import time
import serial
import serial.tools.list_ports

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--mac",required=True)
    p.add_argument("--project",required=True)
    p.add_argument("--profile",required=True)
    p.add_argument("--keys",required=True)
    p.add_argument("--pio",default="pio")
    p.add_argument("--wait",action="store_true")
    args=p.parse_args()
    mac=args.mac.upper()
    while True:
        ports=[p for p in serial.tools.list_ports.comports() if (p.serial_number or "").upper()==mac]
        if len(ports)==1:break
        if not args.wait:raise SystemExit("Expected exactly one board with requested MAC")
        print("Waiting for USB board "+mac,flush=True);time.sleep(5)
    project=Path(args.project)
    log=project/("flash-"+mac.replace(":","").lower()+".log")
    with log.open("w") as f:
        r=subprocess.run([args.pio,"run","-t","upload","--upload-port",ports[0].device],cwd=project,stdout=f,stderr=subprocess.STDOUT)
    print(log.read_text()[-350:],flush=True)
    if r.returncode:raise SystemExit(r.returncode)
    profile=json.loads(Path(args.profile).read_text())
    keys=json.loads(Path(args.keys).read_text())
    profile["ota_password"]=keys[mac]
    profile["low_tx_power"]=True
    port=serial.Serial();port.port=ports[0].device;port.baudrate=115200
    port.timeout=.5;port.dtr=False;port.rts=False;port.open()
    sent=False;last_print=0
    while True:
        port.write(b"status\n")
        try:
            d=json.loads(port.readline())
            if d.get("schema")=="rover.tof.anchor/1":
                if not sent:
                    port.write((json.dumps(profile)+"\n").encode());sent=True
                if d.get("connected") and d.get("ota_available") and d.get("low_tx_power"):
                    print("ANCHOR_READY",json.dumps(d),flush=True);break
        except ValueError:pass
        if time.monotonic()-last_print>15:
            print("Waiting for normal boot / association on "+mac,flush=True);last_print=time.monotonic()
        time.sleep(.5)
    port.close()
    (project/("anchor-"+mac.replace(":","").lower()+"-proof.json")).write_text(json.dumps(d,indent=2))
if __name__=="__main__":main()
