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
    def __init__(self,baseline=None):
        self.baseline=baseline
        self.nodes={}
        self.rover={}
        self.anchor_limit=12

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
            ranges[target]={"distance_m":r["distance_m"],"observed":now-age/1000,
                            "rtt_ns":r.get("rtt_ns"),"samples":r.get("samples"),
                            "sequence":r.get("range_seq"),"latest_attempt_status":r.get("status")}
        self.nodes[node]={"identity":identity,"received":now,"packet":packet,"ranges":ranges}
        if self.baseline is not None:self.baseline.observe(packet)
        return True

    def ingest_rover(self,ftm,received=None):
        now=time.monotonic() if received is None else received
        if ftm.get("busy") or ftm.get("retiring") or ftm.get("status")!="success":return False
        target=str(ftm.get("target","")).lower()
        age=ftm.get("age_ms")
        if target not in self.nodes or not finite_positive(ftm.get("distance_m")):return False
        if not isinstance(age,(int,float)) or age<0 or age>TTL*1000:return False
        self.rover[target]={"distance_m":ftm["distance_m"],"observed":now-age/1000,
                           "samples":ftm.get("samples"),"rtt_ns":ftm.get("rtt_ns")}
        return True

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
        for k,v in self.rover.items():
            if k in active and now-v["observed"]<TTL:result["rover_ranges"].append(dict(v,target_bssid=k,age_s=now-v["observed"]))
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
            r=self.rover.get(k)
            if not r or now-r["observed"]>=TTL:result["status"]="awaiting_rover_ranges";return result
            rr.append(r["distance_m"])
        sample_times=[self.rover[k]["observed"] for k in ids]
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
        result.update(localize_relative(baseline,result["rover_ranges"]))
        return result
