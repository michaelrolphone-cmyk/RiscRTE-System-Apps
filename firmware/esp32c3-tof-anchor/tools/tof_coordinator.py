#!/usr/bin/env python3
"""LAN anchor collector + asynchronous rover FTM scheduler.
Writes one bounded latest-state JSON, never an unbounded measurement journal.
"""
import argparse
import atexit
import signal
import threading
import json
import os
from pathlib import Path
import socket
import time
import urllib.request
import urllib.error
from tof_geometry import RangeGraph
from tof_baseline import AnchorBaseline
from tof_rover import RoverRefinement
from tof_inertial import InertialNavigator, InertialRefinement, VisionFeed
from tof_anchor_packets import AnchorReports, PushAnchorRegistry
from tof_discovery import publish_responders
from tof_gateway_routes import GatewayRoutes
from functools import partial
from tof_runtime import OwnerLock, RoverRequest
from tof_host_io import (configure, configure_survey, AnchorSocket, RuntimeHealth,
                         native_output_clock, wall_observed, prepare)

def http_json(base,path,payload=None,route_registry=None,device_id=None,host_io=None):
    if host_io is not None:
        if not device_id:raise ValueError("portable device identity is required")
        return host_io.device_request(device_id,path,payload)
    if route_registry and device_id:base=GatewayRoutes(route_registry).origin(base,device_id)
    data=None if payload is None else json.dumps(payload).encode()
    request=urllib.request.Request(base+path,data=data,headers={"Content-Type":"application/json"})
    with urllib.request.urlopen(request,timeout=3) as response:
        return json.load(response)

def choose_target(nodes, last_requested):
    # Rotating an index over a changing idle list can repeatedly skip a node.
    # Request age is stable across eligibility changes and failed measurements.
    return min(nodes, key=lambda node: (
        last_requested.get(node["bssid"].lower(), float("-inf")),
        node["bssid"].lower()))

def camera_metadata(camera, device_id, routes, error):
    """Route metadata may fail without abandoning the solver's live custody."""
    origin=camera
    try:
        if routes and device_id:origin=routes.origin(camera,device_id)
    except (OSError,ValueError):
        # Keep the registered reference, not a cached or guessed device address.
        # Commands still resolve through camera_request with their device identity.
        error=error or "camera_route_unavailable"
    return dict(camera=origin,camera_error=error)

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--camera",required=True)
    parser.add_argument("--state",required=True)
    parser.add_argument("--device-routes")
    parser.add_argument("--camera-id")
    parser.add_argument("--port",type=int,default=7446)
    parser.add_argument("--rover-height",type=float,default=.0635)
    parser.add_argument("--vision-state",default=str(Path.home()/".local/share/biome-robot-vision"))
    parser.add_argument("--host-io-config")
    parser.add_argument("--health-port",type=int)
    parser.add_argument("--health-listen",default="127.0.0.1")
    parser.add_argument("--survey-port",type=int)
    parser.add_argument("--survey-listen",default="127.0.0.1")
    parser.add_argument("--survey-token-file")
    parser.add_argument("--prepare-only",action="store_true")
    parser.add_argument("--check",action="store_true")
    args=parser.parse_args()
    host_io=configure(args.host_io_config)
    if host_io is not None and not args.camera_id:
        parser.error("--camera-id is required with --host-io-config")
    # Preparation remains inert. Explicit direct deployment activates only
    # after the previous native owner has stopped and state is bound.
    if args.check or args.prepare_only or os.environ.get("BIOME_SERVICE_PHASE")=="preparing":
        return prepare(args)
    state=Path(args.state);state.mkdir(parents=True,exist_ok=True)
    owner=OwnerLock(state/"coordinator.owner.lock").acquire()
    atexit.register(owner.close)
    stopping=threading.Event()
    signal.signal(signal.SIGTERM,lambda *_:stopping.set())
    signal.signal(signal.SIGINT,lambda *_:stopping.set())
    configure_survey(args.survey_port,args.survey_listen,args.survey_token_file)
    health=RuntimeHealth(args.health_port,args.health_listen)
    if host_io is not None:
        sock=AnchorSocket(host_io)
    else:
        sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
        sock.bind(("",args.port))
    sock.settimeout(.2)
    routes=GatewayRoutes(args.device_routes) if args.device_routes or host_io else None
    camera_request=partial(http_json,args.camera,route_registry=args.device_routes,
                           device_id=args.camera_id,host_io=host_io)
    transport=PushAnchorRegistry(state/"anchor-registry.json",routes=routes)
    # Last observed endpoints are discovery seeds, not accepted measurements.
    try:
        for anchor in json.loads((state/"latest.json").read_text()).get("anchors",[]):
            transport.register(anchor.get("bssid"),anchor.get("ip"))
    except (OSError,ValueError,TypeError):pass
    reports=AnchorReports()
    baseline=AnchorBaseline(state/"baseline.json")
    rover_refinement=RoverRefinement(state/"rover-refinement.json")
    vision_dir = Path(args.vision_state)
    nav = InertialNavigator(vision_dir)
    inertial = InertialRefinement(nav)
    vision_feed=VisionFeed(nav, vision_dir/"vision.sock", host_io=host_io)
    vision_feed.start()
    graph=RangeGraph(baseline=baseline,rover_refinement=rover_refinement,inertial=inertial)
    from tof_dual_radio import DualRadioRanging
    if (vision_dir/'camera-rig.json').is_file() and (vision_dir/'survey-world.json').is_file():
        graph.dual_radio=nav.dual_radio=DualRadioRanging(vision_dir,nav)

    last_write=last_request=last_discovery=0.
    last_sample=None
    discovery_status={}
    channel_cache={}
    last_requested={};error=None;request_state=RoverRequest(
        state/"request-custody.json",state/"latest.json");retired=False
    while not stopping.is_set():
        now=time.monotonic()
        try:
            payload,addr=sock.recvfrom(16384)
            now=getattr(sock,"received_at",None) or time.monotonic()
            if getattr(sock,"reset_required",False):reports.partial.clear()
            packet=reports.receive(addr,payload,now)
            if packet is not None:
                packet["ip"]=addr[0]
                transport.note_udp(packet,addr[0],now)
                graph.ingest(packet,now)
        except (socket.timeout,ValueError,TypeError):pass
        reading=vision_feed.take_ranging()
        if reading is not None:
            snapshot,observed=reading
            now=time.monotonic()
            last_sample=observed
            try:
                if request_state.observe(snapshot):
                    # Do not combine measurements from different camera boots.
                    graph.rover.clear()
                rover_refinement.observe_boot(snapshot.get("boot_id"))
                rover_refinement.observe_motion(snapshot.get("ahrs",{}),now=wall_observed(observed))
                try:
                    visual=json.loads((vision_dir/"rover-motion.json").read_text())
                    rover_refinement.observe_visual(visual)
                except (OSError,ValueError,TypeError):pass
                graph.observe_rover_responder(snapshot,observed)
                ftm=snapshot.get("ftm",{})
                for observation in snapshot.get("wifi",[]):
                    address=str(observation.get("address") or observation.get("bssid") or observation.get("mac") or "").lower()
                    channel=observation.get("channel")
                    if address and isinstance(channel,int) and 1<=channel<=14:
                        channel_cache[address]=channel
                if ftm.get("status")=="success" and isinstance(ftm.get("channel"),int):
                    channel_cache[str(ftm.get("target","")).lower()]=ftm["channel"]
                graph.ingest_rover(ftm,observed)
                # Existing camera requests from other consumers are never cancelled.
                # A stuck FTM driver is reported, not reset under another controller.
                if request_state.pending and now-request_state.started>15:
                    error="rover_ftm_pending";retired=True
                if not ftm.get("busy") and not ftm.get("retiring") and request_state.pending is None and now-last_request>=2:
                    nodes=[v["packet"] for k,v in sorted(graph.nodes.items()) if now-v["received"]<30 and v["packet"].get("ftm_state")=="idle"]
                    if nodes:
                        target=choose_target(nodes,last_requested)
                        address=target["bssid"].lower()
                        request_state.submitted(address,now)
                        last_requested[address]=now;last_request=now;retired=False
                        camera_request("/api/telemetry/ftm",{"address":address,"channel":channel_cache.get(address,target["channel"])})
                if not retired:error=None
            except (OSError,ValueError,urllib.error.HTTPError) as e:
                error=type(e).__name__+": "+str(e)
                # A timed-out POST has uncertain effect. Observe status before retry.
                last_request=now
        if getattr(graph,'dual_radio',None) is not None:
            graph.dual_radio.tick(graph,time.monotonic())
        if now-last_write>=1:
            last_write=now
            out=graph.snapshot(now,args.rover_height)
            if getattr(graph,'dual_radio',None) is not None:
                out['dual_radio']=graph.dual_radio.status()
            inertial.publish(out,time.time())
            out["rover_measurements"]={"transport":"websocket_push","age_s":None if last_sample is None else max(0.,now-last_sample),"boot_id":request_state.boot_id}
            out["anchor_transport"]=transport.status()
            if now-last_discovery>=5:
                last_discovery=now
                discovery_status=publish_responders(sock,out,args.port)
            out["responder_discovery"]=discovery_status
            out.update(updated_at=time.time(),pending_target=request_state.pending,camera_boot_id=request_state.boot_id,request_age_s={k:now-v for k,v in last_requested.items()})
            out.update(camera_metadata(args.camera,args.camera_id,routes,error))
            if host_io is not None:
                try:out=native_output_clock(out,host_io)
                except (OSError,ValueError):
                    # Never publish Linux monotonic values under legacy native fields.
                    health.clock_qualified=False
                    continue
            temp=state/"latest.json.tmp";temp.write_text(json.dumps(out,allow_nan=False))
            os.replace(temp,state/"latest.json")
            health.note(out,nav,host_io)
if __name__=="__main__":main()
