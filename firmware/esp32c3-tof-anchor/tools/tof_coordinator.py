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
from tof_baseline import AnchorBaseline

def http_json(base,path,payload=None):
    data=None if payload is None else json.dumps(payload).encode()
    request=urllib.request.Request(base+path,data=data,headers={"Content-Type":"application/json"})
    with urllib.request.urlopen(request,timeout=3) as response:
        return json.load(response)

class RoverRequest:
    """Reconcile local request custody with the camera's authoritative boot."""
    def __init__(self):
        self.boot_id = None
        self.uptime_ms = None
        self.pending = None
        self.started = 0.

    def observe(self, snapshot):
        ftm = snapshot.get("ftm", {})
        boot = snapshot.get("boot_id")
        uptime = snapshot.get("uptime_ms")
        rebooted = ((boot is not None and self.boot_id is not None and boot != self.boot_id)
                    or (isinstance(uptime, (int, float)) and isinstance(self.uptime_ms, (int, float))
                        and uptime < self.uptime_ms))
        if boot is not None:
            self.boot_id = boot
        if isinstance(uptime, (int, float)):
            self.uptime_ms = uptime
        idle = not ftm.get("busy") and not ftm.get("retiring")
        reset = idle and ftm.get("status") == "never_requested" and not ftm.get("target")
        completed = idle and self.pending and str(ftm.get("target") or "").lower() == self.pending
        if rebooted or reset or completed:
            self.pending = None
        return rebooted

    def submitted(self, address, now):
        self.pending = address.lower()
        self.started = now

def choose_target(nodes, last_requested):
    # Rotating an index over a changing idle list can repeatedly skip a node.
    # Request age is stable across eligibility changes and failed measurements.
    return min(nodes, key=lambda node: (
        last_requested.get(node["bssid"].lower(), float("-inf")),
        node["bssid"].lower()))

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
    baseline=AnchorBaseline(state/"baseline.json")
    graph=RangeGraph(baseline=baseline)
    last_rover=last_write=last_request=0.
    channel_cache={}
    last_requested={};error=None;request_state=RoverRequest();retired=False
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
                if request_state.observe(snapshot):
                    # Do not combine measurements from different camera boots.
                    graph.rover.clear()
                ftm=snapshot.get("ftm",{})
                for observation in snapshot.get("wifi",[]):
                    address=str(observation.get("address") or observation.get("bssid") or observation.get("mac") or "").lower()
                    channel=observation.get("channel")
                    if address and isinstance(channel,int) and 1<=channel<=14:
                        channel_cache[address]=channel
                if ftm.get("status")=="success" and isinstance(ftm.get("channel"),int):
                    channel_cache[str(ftm.get("target","")).lower()]=ftm["channel"]
                graph.ingest_rover(ftm,now)
                # Existing camera requests from other consumers are never cancelled.
                # A stuck FTM driver is reported, not reset under another controller.
                if request_state.pending and now-request_state.started>15:
                    error="rover_ftm_pending";retired=True
                if not ftm.get("busy") and not ftm.get("retiring") and request_state.pending is None and now-last_request>=2:
                    nodes=[v["packet"] for k,v in sorted(graph.nodes.items()) if now-v["received"]<30 and v["packet"].get("ftm_state")=="idle"]
                    if nodes:
                        target=choose_target(nodes,last_requested)
                        address=target["bssid"].lower()
                        http_json(args.camera,"/api/telemetry/ftm",{"address":address,"channel":channel_cache.get(address,target["channel"])})
                        request_state.submitted(address,now);last_requested[address]=now;last_request=now;retired=False
                if not retired:error=None
            except (OSError,ValueError,urllib.error.HTTPError) as e:
                error=type(e).__name__+": "+str(e)
                # A timed-out POST has uncertain effect. Observe status before retry.
                last_request=now
        if now-last_write>=1:
            last_write=now
            out=graph.snapshot(now,args.rover_height)
            out.update(updated_at=time.time(),camera=args.camera,camera_error=error,pending_target=request_state.pending,camera_boot_id=request_state.boot_id,request_age_s={k:now-v for k,v in last_requested.items()})
            temp=state/"latest.json.tmp";temp.write_text(json.dumps(out,allow_nan=False))
            os.replace(temp,state/"latest.json")
if __name__=="__main__":main()
