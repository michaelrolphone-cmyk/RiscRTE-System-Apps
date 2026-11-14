"""FTM anchor geometry and rover trilateration. All lengths are meters.
The local frame is A=(0,0), B=(positive x,0), C=(x,positive y).
This fixes a gauge, not world-map orientation. No fake pose for missing data.
"""
import itertools
import math
import statistics
import time

TTL=30.0

def finite_positive(value):
    return isinstance(value,(int,float)) and not isinstance(value,bool) and math.isfinite(value) and value>0

def horizontal(distance, za, zb):
    if not finite_positive(distance) or not all(isinstance(z,(int,float)) and math.isfinite(z) for z in [za,zb]):
        raise ValueError("invalid distance or height")
    q=distance*distance-(za-zb)**2
    if q<=0: raise ValueError("distance shorter than height difference")
    return math.sqrt(q)

def triangle(ab,ac,bc):
    if not all(finite_positive(v) for v in [ab,ac,bc]):raise ValueError("invalid edge")
    x=(ac*ac+ab*ab-bc*bc)/(2*ab)
    y2=ac*ac-x*x
    if y2<=0:raise ValueError("inconsistent or collinear triangle")
    y=math.sqrt(y2)
    quality=2*ab*y/(ab*ab+ac*ac+bc*bc)
    if quality<0.05:raise ValueError("ill-conditioned triangle")
    return [(0.,0.),(ab,0.),(x,y)],quality

def trilaterate(points,ranges):
    if len(points)!=3 or len(ranges)!=3 or not all(finite_positive(r) for r in ranges):raise ValueError("three valid ranges required")
    a,b,c=points
    aa,ab=2*(b[0]-a[0]),2*(b[1]-a[1])
    ba,bb=2*(c[0]-a[0]),2*(c[1]-a[1])
    u=ranges[0]**2-ranges[1]**2+b[0]**2+b[1]**2-a[0]**2-a[1]**2
    v=ranges[0]**2-ranges[2]**2+c[0]**2+c[1]**2-a[0]**2-a[1]**2
    det=aa*bb-ab*ba
    if abs(det)<1e-8:raise ValueError("singular geometry")
    x,y=(u*bb-ab*v)/det,(aa*v-u*ba)/det
    # Refine against actual distances, rather than squared distances.
    for _ in range(12):
        hxx=hxy=hyy=gx=gy=0.
        for (px,py),r in zip(points,ranges):
            d=max(math.hypot(x-px,y-py),1e-6)
            jx,jy=(x-px)/d,(y-py)/d
            e=d-r;hxx+=jx*jx;hxy+=jx*jy;hyy+=jy*jy;gx+=jx*e;gy+=jy*e
        hdet=hxx*hyy-hxy*hxy
        if hdet<1e-8:raise ValueError("unobservable rover position")
        dx,dy=(hyy*gx-hxy*gy)/hdet,(hxx*gy-hxy*gx)/hdet
        x-=dx;y-=dy
        if math.hypot(dx,dy)<1e-6:break
    residuals=[math.hypot(x-px,y-py)-r for (px,py),r in zip(points,ranges)]
    rms=math.sqrt(sum(e*e for e in residuals)/3)
    return {"x_m":x,"y_m":y,"residual_rms_m":rms,"residuals_m":residuals}

class RangeGraph:
    def __init__(self,baseline=None,rover_refinement=None):
        self.rover_refinement=rover_refinement
        self.baseline=baseline
        self.nodes={}
        self.rover={}
        self.anchor_limit=12
        self.rover_responder = None
        self.rover_boot = None
        self.rover_boot_started = None
        self.responder_received = None

    def ingest(self,packet,received=None):
        now=time.monotonic() if received is None else received
        if packet.get("schema")!="rover.tof.anchor/1":return False
        node=str(packet.get("bssid","")).lower()
        if len(node)!=17 or not packet.get("ftm_responder"):return False
        if node not in self.nodes and len(self.nodes)>=self.anchor_limit:return False
        prev=self.nodes.get(node)
        identity=(packet.get("boot_id"),packet.get("seq"))
        if prev and prev["identity"][0]==identity[0] and identity[1]<=prev["identity"][1]:return False
        # Retain the last real successful observation while a new attempt is
        # pending/failed. Its timestamp never refreshes without a new success.
        ranges={}
        if prev and prev["identity"][0]==identity[0]:
            ranges={k:dict(v) for k,v in prev["ranges"].items() if now-v["observed"]<TTL}
        for r in packet.get("ranges",[])[:12]:
            target=str(r.get("target_bssid","")).lower()
            if target in ranges:ranges[target]["latest_attempt_status"]=r.get("status")
            age=r.get("age_ms")
            if not r.get("valid") or r.get("status")!="ok" or not finite_positive(r.get("distance_m")):continue
            if not isinstance(age,(int,float)) or age<0 or age>TTL*1000:continue
            target=str(r.get("target_bssid","")).lower()
            prior=ranges.get(target)
            if prior and r.get("range_seq") is not None and prior.get("sequence")==r["range_seq"]:
                continue  # Re-publishing a report never refreshes its observation time.
            ranges[target]={"distance_m":r["distance_m"],"observed":now-age/1000,
                            "rtt_ns":r.get("rtt_ns"),"samples":r.get("samples"),
                            "sequence":r.get("range_seq"),"latest_attempt_status":r.get("status")}
            if target==self.rover_responder:
                self._refine_report(node,ranges[target],"anchor_to_rover",now,identity[0])
        self.nodes[node]={"identity":identity,"received":now,"packet":packet,"ranges":ranges}
        if self.baseline is not None:
            # The mobile rover is never part of fixed-anchor refinement.
            fixed=dict(packet,ranges=[r for r in packet.get("ranges",[])[:12]
                if str(r.get("target_bssid","")).lower()!=self.rover_responder
                and not str(r.get("ssid","")).startswith("C3-TOF-ROVER-")])
            self.baseline.observe(fixed)
        return True

    def observe_rover_responder(self, snapshot, received=None):
        now=time.monotonic() if received is None else received
        responder=snapshot.get("ftm_responder") or {}
        if not isinstance(responder,dict):
            self.rover_responder=None
            return False
        mac=str(responder.get("bssid","")).lower()
        if (not responder.get("active") or not responder.get("station_connected")
                or len(mac)!=17 or mac=="00:00:00:00:00:00"):
            self.rover_responder=None
            return False
        boot=snapshot.get("boot_id")
        uptime=snapshot.get("uptime_ms")
        if boot != self.rover_boot:
            self.rover_boot_started=now-uptime/1000 if isinstance(uptime,(int,float)) else now
            self.rover_boot=boot
        self.rover_responder=mac
        self.responder_received=now
        return True

    def ingest_rover(self,ftm,received=None):
        now=time.monotonic() if received is None else received
        if ftm.get("busy") or ftm.get("retiring") or ftm.get("status")!="success":return False
        target=str(ftm.get("target","")).lower()
        age=ftm.get("age_ms")
        if target not in self.nodes or not finite_positive(ftm.get("distance_m")):return False
        if not isinstance(age,(int,float)) or age<0 or age>TTL*1000:return False
        previous=self.rover.get(target)
        if (previous and ftm.get("report_id") is not None
                and previous.get("sequence")==ftm["report_id"]):return False
        self.rover[target]={"distance_m":ftm["distance_m"],"observed":now-age/1000,
                           "samples":ftm.get("samples"),"rtt_ns":ftm.get("rtt_ns"),
                           "sequence":ftm.get("report_id")}
        self._refine_report(target,self.rover[target],"rover_to_anchor",now,self.rover_boot)
        return True

    def _refine_report(self, anchor, report, direction, now, source_boot):
        if self.rover_refinement is None:return
        if direction=="anchor_to_rover":
            if (self.rover_responder is None or self.responder_received is None
                    or now-self.responder_received>=TTL
                    or (self.rover_boot_started is not None and report["observed"]<self.rover_boot_started)):
                return
            other=self.rover.get(anchor)
        else:
            other=self.nodes.get(anchor,{}).get("ranges",{}).get(self.rover_responder)
        if other and abs(report["observed"]-other["observed"])<=2.:
            center=(report["distance_m"]+other["distance_m"])/2
            if abs(report["distance_m"]-other["distance_m"])>max(1.,.25*center):return
        wall=time.time()
        sequence=report.get("sequence")
        token=str(source_boot)+":"+str(sequence) if sequence is not None else None
        self.rover_refinement.observe(anchor,report["distance_m"],wall-(now-report["observed"]),
                                      now=wall,direction=direction,session_id=token)

    def rover_observations(self, active, now):
        combined=[];directional=[]
        responder_fresh=(self.rover_responder is not None and self.responder_received is not None
                         and now-self.responder_received<TTL)
        for anchor,node in sorted(active.items()):
            reports=[]
            forward=self.rover.get(anchor)
            if forward and 0<=now-forward["observed"]<TTL:
                reports.append(dict(forward,direction="rover_to_anchor",
                                    source_bssid=self.rover_responder,measured_target_bssid=anchor,
                                    source_boot_id=self.rover_boot))
            reverse=node["ranges"].get(self.rover_responder) if responder_fresh else None
            if (reverse and 0<=now-reverse["observed"]<TTL and
                    (self.rover_boot_started is None or reverse["observed"]>=self.rover_boot_started)):
                reports.append(dict(reverse,direction="anchor_to_rover",source_bssid=anchor,
                                    measured_target_bssid=self.rover_responder,
                                    source_boot_id=node["identity"][0]))
            if not reports:continue
            directional.extend(dict(r,target_bssid=anchor,age_s=now-r["observed"]) for r in reports)
            newest=max(reports,key=lambda r:r["observed"])
            selected=dict(newest,target_bssid=anchor,age_s=now-newest["observed"],
                          directions=len(reports),method="single_direction",consistent=True)
            if len(reports)==2:
                distances=[r["distance_m"] for r in reports]
                skew=abs(reports[0]["observed"]-reports[1]["observed"])
                disagreement=abs(distances[0]-distances[1])
                selected.update(disagreement_m=disagreement,time_skew_s=skew)
                if skew<=2.:
                    distance=statistics.median(distances)
                    good=disagreement<=max(1.,.25*distance)
                    # One geometric constraint per anchor. Correlated reciprocal
                    # reports do not pretend to be two independent anchors.
                    observed=min(r["observed"] for r in reports)
                    selected.update(distance_m=distance,observed=observed,age_s=now-observed,
                                    method="reciprocal_median",consistent=good)
                else:
                    selected.update(method="newest_direction",consistent=None)
            selected["directional_measurements"]=[dict(r,age_s=now-r["observed"]) for r in reports]
            combined.append(selected)
        return combined,directional

    def _snapshot_instantaneous(self,now=None,rover_height_m=0.0635):
        now=time.monotonic() if now is None else now
        active={k:v for k,v in self.nodes.items() if now-v["received"]<TTL}
        result={"schema":"rover.tof.solution/1","status":"insufficient_anchors","valid":False,
                "anchor_count":len(active),"anchors":[dict(v["packet"],received_age_s=now-v["received"]) for v in active.values()],
                "frame":{"kind":"relative_anchor_frame","world_alignment":"unregistered","orientation":"A to B is +X; C is +Y side"},
                "rover_height_m":rover_height_m,"position":None,"edges":[],"rover_ranges":[]}
        edges={}
        for a,b in itertools.combinations(sorted(active),2):
            obs=[]
            for src,dst in [(a,b),(b,a)]:
                r=active[src]["ranges"].get(dst)
                if r and now-r["observed"]<TTL:obs.append(r["distance_m"])
            if obs:
                d=statistics.median(obs);disagreement=max(obs)-min(obs)
                good=disagreement<=max(1.,0.25*d)
                result["edges"].append({"a":a,"b":b,"distance_m":d,"directions":len(obs),"disagreement_m":disagreement,"consistent":good})
                if good:edges[a,b]=d
        result["rover_ranges"],result["rover_directional_ranges"]=self.rover_observations(active,now)
        result["rover_responder_bssid"]=self.rover_responder
        result["reciprocal_ftm"]={"forward_count":sum(r["direction"]=="rover_to_anchor" for r in result["rover_directional_ranges"]),
                                  "reverse_count":sum(r["direction"]=="anchor_to_rover" for r in result["rover_directional_ranges"]),
                                  "paired_count":sum(r["directions"]==2 for r in result["rover_ranges"]),
                                  "inconsistent_count":sum(r.get("consistent") is False for r in result["rover_ranges"]),
                                  "max_pair_skew_s":2.,"mode":"reciprocal_not_simultaneous_full_duplex"}
        rover={r["target_bssid"]:r for r in result["rover_ranges"] if r.get("consistent") is not False}
        if len(active)<3:return result
        candidates=[]
        heights_known=all(v["packet"].get("height_known") and isinstance(v["packet"].get("height_m"),(int,float)) for v in active.values())
        for ids in itertools.combinations(sorted(active),3):
            try:
                z=[active[k]["packet"].get("height_m",0.) for k in ids]
                ds=[edges[tuple(sorted((ids[i],ids[j])))] for i,j in [(0,1),(0,2),(1,2)]]
                if heights_known:ds=[horizontal(d,z[i],z[j]) for d,(i,j) in zip(ds,[(0,1),(0,2),(1,2)])]
                points,quality=triangle(*ds);candidates.append((quality,ids,points,z))
            except (KeyError,ValueError):continue
        if not candidates:result["status"]="incomplete_or_inconsistent_anchor_geometry";return result
        quality,ids,points,z=max(candidates)
        result["anchor_geometry"]=[{"bssid":k,"x_m":p[0],"y_m":p[1],"height_m":h if heights_known else None} for k,p,h in zip(ids,points,z)]
        result["geometry_quality"]=quality
        result["height_mode"]="configured" if heights_known else "assumed_coplanar_for_preview"
        rr=[]
        for k in ids:
            r=rover.get(k)
            if not r or now-r["observed"]>=TTL:result["status"]="awaiting_rover_ranges";return result
            rr.append(r["distance_m"])
        sample_times=[rover[k]["observed"] for k in ids]
        result["rover_range_time_span_s"]=max(sample_times)-min(sample_times)
        result["motion_compensated"]=False
        result["moving_rover_requires_motion_compensation"]=True
        if not heights_known:
            # Three spheres produce two points reflected through the anchor plane.
            ab=points[1][0];cx,cy=points[2]
            x=(rr[0]**2-rr[1]**2+ab**2)/(2*ab)
            y=(rr[0]**2-rr[2]**2+cx**2+cy**2-2*cx*x)/(2*cy)
            z2=rr[0]**2-x*x-y*y
            result["height_mode"]="anchor_plane_3d"
            if z2<0:
                result["status"]="inconsistent_3d_ranges"
            else:
                z=math.sqrt(z2)
                result["position_hypotheses"]=[{"x_m":x,"y_m":y,"z_m":z},{"x_m":x,"y_m":y,"z_m":-z}]
                result["status"]="mirror_ambiguous_3d"
                result["ambiguity"]="Anchor ranges determine the plane but cannot choose its normal or register the world map."
            return result
        try:
            rs=[horizontal(r,rover_height_m,h) for r,h in zip(rr,z)]
            pos=trilaterate(points,rs)
            result["position"]=dict(pos,z_m=rover_height_m)
            threshold=max(.75,.15*statistics.median(rs))
            result["residual_acceptance_m"]=threshold
            result["valid"]=pos["residual_rms_m"]<=threshold
            result["status"]="relative_position" if result["valid"] else "inconsistent_rover_ranges"
            result["navigation_authority"]=False
            result["uncertainty"]="FTM biases require physical calibration; residual is not an accuracy bound"
        except ValueError as e:result["status"]=str(e)
        return result

    def snapshot(self,now=None,rover_height_m=0.0635):
        result=self._snapshot_instantaneous(now,rover_height_m)
        if self.baseline is None:return result
        from tof_baseline import localize_relative
        baseline=self.baseline.update()
        result["baseline"]=baseline
        result["frame"]={"kind":"relative_anchor_frame","world_alignment":"unregistered",
                         "frame_id":baseline["frame_id"],"baseline_epoch":baseline["epoch"],"baseline_revision":baseline["revision"],
                         "orientation":"Persistent relative gauge; not registered to floor or world"}
        result["anchor_geometry"]=baseline["positions"]
        result["height_mode"]="relative_3d" if baseline["dimension"]==3 else "relative_anchor_plane"
        result.pop("position_hypotheses",None);result.pop("ambiguity",None)
        result.update(valid=False,position=None,navigation_authority=False)
        if not baseline["valid"]:
            result["status"]="baseline_"+baseline["status"]
            return result
        result["geometry_quality"]=None if baseline["condition_number"] is None else 1./baseline["condition_number"]
        if self.rover_refinement is not None:
            refinement=self.rover_refinement.snapshot(baseline)
            result["rover_refinement"]=refinement
            excluded={r["target_bssid"] for r in result["rover_ranges"] if r.get("consistent") is False}
            refined=[r for r in refinement.get("ranges",[]) if r["target_bssid"] not in excluded]
            if excluded:
                refinement["range_exclusions"]=sorted(excluded)
                refinement["estimate"]=None
            result.update(localize_relative(baseline,refined))
            if refinement.get("state")=="moving":
                result.update(valid=False,position=None,status="rover_moving_reacquire")
            elif refinement.get("minimum_sessions_per_anchor",0)<5:
                result.update(valid=False,position=None,status="rover_refinement_accumulating")
            elif result.get("valid"):
                result["status"]="refined_relative_position"
            if result.get("position"):
                result["position"]["frame_id"]=baseline["frame_id"]
                result["position"]["refinement_segment"]=refinement["segment"]
        else:
            result.update(localize_relative(baseline,result["rover_ranges"]))
        return result
