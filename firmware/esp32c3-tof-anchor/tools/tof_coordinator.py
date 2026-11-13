#!/usr/bin/env python3
"""LAN anchor collector + asynchronous rover FTM scheduler.
Writes one bounded latest-state JSON, never an unbounded measurement journal.
"""
import argparse
import json
import os
from pathlib import Path
import socket
import time
import urllib.request
import urllib.error
from tof_geometry import RangeGraph

def http_json(base,path,payload=None):
    data=None if payload is None else json.dumps(payload).encode()
    request=urllib.request.Request(base+path,data=data,headers={"Content-Type":"application/json"})
    with urllib.request.urlopen(request,timeout=3) as response:
        return json.load(response)

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--camera",required=True)
    parser.add_argument("--state",required=True)
    parser.add_argument("--port",type=int,default=7446)
    parser.add_argument("--rover-height",type=float,default=.0635)
    args=parser.parse_args()
    state=Path(args.state);state.mkdir(parents=True,exist_ok=True)
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
    sock.bind(("",args.port));sock.settimeout(.2)
    graph=RangeGraph()
    last_rover=last_write=last_request=0.
    cursor=0;error=None;pending=None;pending_started=0.;retired=False
    while True:
        now=time.monotonic()
        try:
            payload,addr=sock.recvfrom(16384)
            packet=json.loads(payload)
            # Packet source supplies its reachability address, not an arbitrary advertised host.
            packet["ip"]=addr[0]
            graph.ingest(packet,now)
        except (socket.timeout,ValueError,TypeError):pass
        if now-last_rover>=1:
            last_rover=now
            try:
                snapshot=http_json(args.camera,"/api/telemetry")
                ftm=snapshot.get("ftm",{})
                graph.ingest_rover(ftm,now)
                if pending and not ftm.get("busy") and str(ftm.get("target","")).lower()==pending:
                    pending=None
                # Existing camera requests from other consumers are never cancelled.
                # A stuck FTM driver is reported, not reset under another controller.
                if pending and now-pending_started>15:
                    error="rover_ftm_pending";retired=True
                if not ftm.get("busy") and not ftm.get("retiring") and pending is None and now-last_request>=2:
                    nodes=[v["packet"] for k,v in sorted(graph.nodes.items()) if now-v["received"]<30]
                    if nodes:
                        target=nodes[cursor%len(nodes)];cursor+=1
                        address=target["bssid"].lower()
                        http_json(args.camera,"/api/telemetry/ftm",{"address":address,"channel":target["channel"]})
                        pending=address;pending_started=now;last_request=now;retired=False
                if not retired:error=None
            except (OSError,ValueError,urllib.error.HTTPError) as e:
                error=type(e).__name__+": "+str(e)
                # A timed-out POST has uncertain effect. Observe status before retry.
                last_request=now
        if now-last_write>=1:
            last_write=now
            out=graph.snapshot(now,args.rover_height)
            out.update(updated_at=time.time(),camera=args.camera,camera_error=error,pending_target=pending)
            temp=state/"latest.json.tmp";temp.write_text(json.dumps(out,allow_nan=False))
            os.replace(temp,state/"latest.json")
if __name__=="__main__":main()
