"""Persistent relative FTM geometry; independent of floor/world registration.

Only distinct successful ranging sessions enter the bounded rolling window.
A saved fit is frozen until sustained multi-link evidence invalidates it.
Reported covariance describes the range model, not calibrated RF accuracy.
"""
import itertools
import json
import math
import os
from pathlib import Path
import statistics
import time

import numpy as np
from scipy.optimize import least_squares

SCHEMA = "rover.tof.baseline/1"

def positive(x):
    return isinstance(x, (int, float)) and not isinstance(x, bool) and math.isfinite(x) and x > 0

def median_sigma(values):
    median = statistics.median(values)
    return median, max(.25, 1.4826 * statistics.median(abs(x-median) for x in values))

def connected_after(nodes, edges, removed):
    remaining = set(nodes)-set(removed)
    if not remaining:
        return True
    reached = {next(iter(remaining))}
    while True:
        more = reached | {b for a,b in edges if a in reached and b in remaining} | {a for a,b in edges if b in reached and a in remaining}
        if more == reached:
            return reached == remaining
        reached = more

def align(points, reference, exclude=(), require_3d=False):
    common = sorted((set(points) & set(reference))-set(exclude))
    if len(common) < 3:
        return points, False
    x = np.array([points[k] for k in common]); y = np.array([reference[k] for k in common])
    xc, yc = x.mean(axis=0), y.mean(axis=0)
    if np.linalg.matrix_rank(x-xc, tol=1e-5) < 2:
        return points, False
    u, _, vt = np.linalg.svd((x-xc).T @ (y-yc))
    rotation = u @ vt
    return {k: ((np.array(v)-xc) @ rotation+yc).tolist() for k,v in points.items()}, bool(np.linalg.matrix_rank(x-xc,tol=1e-5)>=(3 if require_3d else 2))

def fit_geometry(nodes, edges, dimension):
    """Fit measured edges only. Shortest paths are initial guesses, not evidence."""
    n = len(nodes); dof = dimension*n-dimension*(dimension+1)//2
    if n < dimension+1 or len(edges) < dof:
        return None
    if n > dimension+1:
        if len(edges) < dof+1:
            return None
        if any(not connected_after(nodes, [(e["a"],e["b"]) for e in edges], removed)
               for removed in itertools.combinations(nodes, dimension)):
            return None
    index = {k:i for i,k in enumerate(nodes)}
    distances = np.full((n,n), np.inf); np.fill_diagonal(distances, 0)
    for e in edges:
        a,b = index[e["a"]], index[e["b"]]; distances[a,b] = distances[b,a] = e["distance_m"]
    for k in range(n):
        distances = np.minimum(distances, distances[:,k,None]+distances[None,k,:])
    if not np.isfinite(distances).all():
        return None
    center = np.eye(n)-np.ones((n,n))/n
    values, vectors = np.linalg.eigh(-.5*center @ (distances**2) @ center)
    order = np.argsort(values)[::-1][:dimension]
    initial = vectors[:,order]*np.sqrt(np.maximum(values[order], .01))
    # Choose a stable, noncollinear gauge triangle, not whichever packet arrived first.
    triple = max(itertools.combinations(range(n),3),
                 key=lambda t: np.linalg.norm(np.cross(np.pad(initial[t[1]]-initial[t[0]],(0,3-dimension)),
                                                       np.pad(initial[t[2]]-initial[t[0]],(0,3-dimension)))))
    permutation = list(triple)+[i for i in range(n) if i not in triple]
    nodes = [nodes[i] for i in permutation]; initial = initial[permutation]
    index = {k:i for i,k in enumerate(nodes)}
    initial -= initial[0].copy()
    e1 = initial[1]/np.linalg.norm(initial[1])
    e2 = initial[2]-np.dot(initial[2],e1)*e1
    if np.linalg.norm(e2) < 1e-5:
        return None
    e2 /= np.linalg.norm(e2)
    basis = np.stack([e1,e2] if dimension==2 else [e1,e2,np.cross(e1,e2)],axis=1)
    initial = initial @ basis
    slots = [(i,j) for i in range(1,n) for j in range(dimension)
             if not (i==1 and j>0) and not (i==2 and j==2)]
    def unpack(v):
        p = np.zeros((n,dimension))
        for value,(i,j) in zip(v,slots): p[i,j] = value
        return p
    x0 = np.array([initial[i,j] for i,j in slots])
    lower = np.full(len(slots), -np.inf); upper = -lower
    lower[slots.index((1,0))] = .01; lower[slots.index((2,1))] = .01
    x0 = np.maximum(x0, np.where(np.isfinite(lower),lower+.01,-np.inf))
    def residual(v, scaled=True):
        p = unpack(v)
        return np.array([(np.linalg.norm(p[index[e["a"]]]-p[index[e["b"]]])-e["distance_m"]) /
                         (e["sigma_m"] if scaled else 1.) for e in edges])
    best = None
    rng = np.random.default_rng(1746)
    for attempt in range(4):
        start = x0.copy()
        if attempt:
            start += rng.normal(0, max(.2,float(np.median(distances))*.15), len(start))
            start = np.maximum(start,np.where(np.isfinite(lower),lower+.01,-np.inf))
        fit = least_squares(residual,start,bounds=(lower,upper),loss="soft_l1",max_nfev=400)
        cost = float(np.sum(np.minimum(residual(fit.x)**2, 9.)))
        if best is None or cost < best[0]: best = (cost,fit)
    fit = best[1]; p = unpack(fit.x); errors = residual(fit.x,False)
    # Unweighted distance Jacobian; robust loss must not create fake observability.
    jac = np.zeros((len(edges),len(slots)))
    for row,e in enumerate(edges):
        a,b = index[e["a"]],index[e["b"]]; delta = p[a]-p[b]; length = np.linalg.norm(delta)
        if length < 1e-5: return None
        for col,(i,j) in enumerate(slots):
            jac[row,col] = (delta[j]/length)*(int(i==a)-int(i==b))
    singular = np.linalg.svd(jac,compute_uv=False)
    if len(singular)<dof or singular[-1] < max(1e-5,singular[0]*1e-4):
        return None
    # Redundant infinitesimal rigidity + connectivity prevent ordinary flip ambiguities.
    # This is sufficient for generic 2D graphs; 3D sparse graphs also need qualification.
    if n > dimension+1:
        if any(np.linalg.matrix_rank(np.delete(jac,i,axis=0),tol=1e-5)<dof for i in range(len(edges))):
            return None
        if dimension==3 and n>5 and len(edges)<n*(n-1)//2:
            return None  # no general global-rigidity certificate implemented for larger sparse 3D graphs
    limits = np.array([max(.75, .12*e["distance_m"], 2.5*e["sigma_m"]) for e in edges])
    if np.any(np.abs(errors)>limits):
        return None
    covariance = np.linalg.pinv(jac.T @ np.diag([1/e["sigma_m"]**2 for e in edges]) @ jac)
    points = {k: np.pad(p[i],(0,3-dimension)).tolist() for i,k in enumerate(nodes)}
    uncertainty = {}
    for i,k in enumerate(nodes):
        uncertainty[k] = math.sqrt(sum(max(0,float(covariance[c,c])) for c,(a,_) in enumerate(slots) if a==i))
    return {"positions":points,"dimension":dimension,"rank":dof,"condition_number":float(singular[0]/singular[-1]),
            "residual_rms_m":float(np.sqrt(np.mean(errors**2))),
            "anchor_model_sigma_m":uncertainty,
            "edges":[dict(e,residual_m=float(err)) for e,err in zip(edges,errors)]}

class AnchorBaseline:
    def __init__(self,path=None,clock=time.time,min_samples=5,min_span_s=30.,
                 window_s=1800.,recent_s=90.,movement_hold_s=30.,fit_interval_s=10.):
        self.path = Path(path) if path else None; self.clock = clock
        self.min_samples=min_samples; self.min_span_s=min_span_s; self.window_s=window_s
        self.recent_s=recent_s; self.movement_hold_s=movement_hold_s; self.fit_interval_s=fit_interval_s
        self.history={}; self.last_seen={}; self.fit=None; self.previous=[]; self.epoch=0
        self.recalibrating=False; self.moved=[]; self.suspicions={}; self.events=[]
        self.last_fit_attempt=-1e30; self.last_saved=-1e30; self.dirty=False; self.load_error=None
        self.last_rejection=None; self.loaded=False
        if self.path and self.path.exists():
            try:
                value=json.loads(self.path.read_text())
                if value.get("schema")!=SCHEMA: raise ValueError("unsupported baseline schema")
                self.history=value.get("history",{}); self.last_seen=value.get("last_seen",{})
                self.fit=value.get("fit"); self.previous=value.get("previous",[])[-2:]
                self.epoch=int(value.get("epoch",0)); self.recalibrating=bool(value.get("recalibrating"))
                self.moved=value.get("moved",[]); self.events=value.get("events",[])[-16:]
                self.suspicions=value.get("suspicions",{})
                if not isinstance(self.history,dict) or not isinstance(self.last_seen,dict):raise ValueError("invalid history")
                if self.fit:
                    assert all(len(p)==3 and all(math.isfinite(x) for x in p) for p in self.fit["positions"].values())
                self.loaded=True
            except (OSError,ValueError,TypeError,KeyError,AssertionError) as e:
                self.history={};self.last_seen={};self.fit=None;self.suspicions={}
                self.load_error=type(e).__name__+": "+str(e)
        self._prune(self.clock())

    def _prune(self,now):
        for key,rows in list(self.history.items()):
            # Bounded per-direction ring. Wall-clock rollback cannot make future samples fresh.
            rows=[r for r in rows if isinstance(r,list) and len(r)==5 and now-self.window_s<=r[0]<=now+1.]
            if rows:self.history[key]=rows[-120:]
            else:self.history.pop(key,None)
        self.last_seen={k:v for k,v in self.last_seen.items() if now-self.window_s<=v<=now+1.}

    def observe(self,packet,received=None):
        now=self.clock() if received is None else received
        src=str(packet.get("bssid","")).lower()
        if packet.get("schema")!="rover.tof.anchor/1" or not packet.get("ftm_responder") or len(src)!=17:
            return
        self.last_seen[src]=now; self.dirty=True
        for report in packet.get("ranges",[]):
            dst=str(report.get("target_bssid","")).lower(); age=report.get("age_ms"); seq=report.get("range_seq")
            if dst==src or len(dst)!=17 or not report.get("valid") or report.get("status")!="ok":continue
            if not positive(report.get("distance_m")) or not isinstance(age,(int,float)) or not math.isfinite(age) or not 0<=age<=30000:continue
            if not isinstance(seq,int) or isinstance(seq,bool):continue
            key=src+">"+dst; token=str(packet.get("boot_id"))+":"+str(seq)
            rows=self.history.setdefault(key,[])
            if any(r[1]==token for r in rows):continue
            rows.append([now-age/1000.,token,float(report["distance_m"]),report.get("samples",0),now])
        self._prune(now)

    def _edges(self,now,recent=False):
        pairs={}
        for key,rows in self.history.items():
            src,dst=key.split(">")
            selected=[r for r in rows if now-(self.recent_s if recent else self.window_s)<=r[0]<=now+1.]
            minimum=3 if recent else self.min_samples
            if len(selected)<minimum:continue
            span=max(r[0] for r in selected)-min(r[0] for r in selected)
            if not recent and span<self.min_span_s:continue
            d,sigma=median_sigma([r[2] for r in selected])
            pairs.setdefault(tuple(sorted((src,dst))),[]).append((d,sigma,len(selected),span,max(r[0] for r in selected)))
        out=[]
        for (a,b),directions in pairs.items():
            d=statistics.median(v[0] for v in directions)
            sigma=max(.25,statistics.median(v[1] for v in directions))
            disagreement=max(v[0] for v in directions)-min(v[0] for v in directions)
            if disagreement>max(1.,.25*d,3*sigma):continue
            out.append({"a":a,"b":b,"distance_m":d,"sigma_m":sigma,
                        "directions":len(directions),"samples":sum(v[2] for v in directions),
                        "span_s":min(v[3] for v in directions),"last_observed_at":max(v[4] for v in directions)})
        return out

    def _movement(self,now):
        if not self.fit or self.recalibrating:return
        old={tuple(sorted((e["a"],e["b"]))):e for e in self.fit["edges"]}
        changed=[]; stable=[]
        for edge in self._edges(now,True):
            pair=(edge["a"],edge["b"]); before=old.get(pair)
            if not before:continue
            # Compare only post-fit sessions. A frozen historical median is never refreshed by a packet.
            rows=[r for key in [pair[0]+">"+pair[1],pair[1]+">"+pair[0]]
                  for r in self.history.get(key,[]) if now-self.recent_s<=r[0] and r[0]>self.fit["created_at"]]
            if len(rows)<4 or max(r[0] for r in rows)-min(r[0] for r in rows)<self.movement_hold_s:continue
            d,sigma=median_sigma([r[2] for r in rows])
            effective=min(8.,len(rows)/2.)
            threshold=max(1.,.15*before["distance_m"],
                          4*math.sqrt(before["sigma_m"]**2/max(1,min(8,before["samples"]/2.))+sigma**2/effective))
            delta=d-before["distance_m"]
            same=sum((r[2]-before["distance_m"])*delta>0 and abs(r[2]-before["distance_m"])>threshold*.75 for r in rows)
            if abs(delta)>threshold and same>=.8*len(rows):
                changed.append(pair)
            elif abs(delta)<threshold*.5:
                stable.append(pair)
        degree={}
        for a,b in changed:
            degree[a]=degree.get(a,0)+1;degree[b]=degree.get(b,0)+1
        candidates=[]; localized=False
        # The smallest unique set covering the changed links identifies likely moved
        # anchors without naming every stationary neighbour of two moved boards.
        eligible=sorted(k for k,v in degree.items() if v>=2)
        for count in range(1,len(eligible)+1):
            covers=[group for group in itertools.combinations(eligible,count)
                    if all(set(pair)&set(group) for pair in changed)
                    and any(not set(pair)&set(group) for pair in stable)]
            if covers:
                if len(covers)==1:candidates=list(covers[0]);localized=True
                break
        if not candidates:
            if len(changed)<3:
                self.suspicions={};return
            # Multiple moves or correlated RF changes can be indistinguishable.
            # Invalidate and relearn the affected network; do not assert a culprit.
            candidates=sorted(degree)
        signature="|".join(sorted(candidates))
        previous=self.suspicions.get(signature)
        if previous is None:
            self.suspicions={signature:now};self.dirty=True;return
        if now-previous<self.movement_hold_s:return
        self.moved=sorted(candidates);self.recalibrating=True;self.suspicions={}
        self.events.append({"type":"anchor_movement_detected" if localized else "baseline_geometry_change_detected","at":now,"anchors":self.moved,
                            "changed_links":[list(p) for p in changed],"cause":"sustained_range_geometry_change" if localized else "movement_or_persistent_RF_change"})
        self.events=self.events[-16:]
        for key in list(self.history):
            if set(key.split(">")) & set(self.moved):self.history.pop(key)
        self.last_fit_attempt=-1e30;self.dirty=True

    def _calibrate(self,now):
        if now-self.last_fit_attempt<self.fit_interval_s:return
        self.last_fit_attempt=now;edges=self._edges(now)
        nodes=sorted(k for k,last in self.last_seen.items() if now-last<=30)
        if self.fit and not self.recalibrating and set(nodes)<=set(self.fit["positions"]):return
        minimum=3
        if self.fit and not self.recalibrating:minimum=len(self.fit["positions"])+1
        if self.recalibrating and self.fit:minimum=len(self.fit["positions"])
        accepted=None
        for size in range(len(nodes),minimum-1,-1):
            for subset in itertools.combinations(nodes,size):
                selected=[e for e in edges if e["a"] in subset and e["b"] in subset]
                for dimension in ([3,2] if size>=4 else [2]):
                    accepted=fit_geometry(list(subset),selected,dimension)
                    if accepted:break
                if accepted:break
            if accepted:break
        if not accepted:
            self.last_rejection="insufficient_samples_or_unobservable_or_inconsistent_geometry";return
        old=self.fit;aligned=False
        if old:
            accepted["positions"],aligned=align(accepted["positions"],old["positions"],self.moved,require_3d=accepted["dimension"]==3)
        if old and self.recalibrating and not aligned:
            self.last_rejection="insufficient_stationary_anchors_to_preserve_frame"
            # Still recalibrate automatically, but explicitly start a new relative frame.
        self.epoch+=1
        accepted.update(created_at=now,epoch=self.epoch,frame_id=old["frame_id"] if old and aligned else "tof-relative-"+str(self.epoch),
                        aligned_to_previous=aligned)
        if old:self.previous=(self.previous+[{"epoch":old["epoch"],"positions":old["positions"],"created_at":old["created_at"]}])[-2:]
        self.fit=accepted;self.recalibrating=False;self.moved=[]
        self.events.append({"type":"baseline_calibrated","at":now,"epoch":self.epoch,"frame_id":accepted["frame_id"],
                            "anchors":list(accepted["positions"]),"aligned_to_previous":aligned})
        self.events=self.events[-16:];self.last_rejection=None;self.dirty=True

    def update(self,now=None):
        now=self.clock() if now is None else now
        self._prune(now);self._movement(now);self._calibrate(now);self.save(now)
        return self.status(now)

    def status(self,now=None):
        now=self.clock() if now is None else now
        fit=self.fit;active=sorted(k for k,last in self.last_seen.items() if now-last<=30)
        ready=bool(fit) and not self.recalibrating and not self.suspicions
        status="recalibrating" if self.recalibrating else "movement_suspected" if self.suspicions else "ready" if fit else "collecting"
        if ready and set(active)-set(fit["positions"]):status="partial"
        return {"schema":SCHEMA,"status":status,"valid":ready,"epoch":self.epoch,
                "frame_id":fit["frame_id"] if fit else None,"dimension":fit["dimension"] if fit else None,
                "created_at":fit["created_at"] if fit else None,
                "positions":[{"bssid":k,"x_m":v[0],"y_m":v[1],"z_m":v[2],
                              "model_sigma_m":fit["anchor_model_sigma_m"].get(k)} for k,v in sorted(fit["positions"].items())] if fit else [],
                "unfitted_anchors":sorted(set(active)-set(fit["positions"] if fit else [])),
                "movement_anchors":self.moved or sorted({k for s in self.suspicions for k in s.split("|")}),
                "movement_threshold_m":1.,"movement_hold_s":self.movement_hold_s,
                "residual_rms_m":fit["residual_rms_m"] if fit else None,
                "condition_number":fit["condition_number"] if fit else None,
                "sample_count":sum(len(v) for v in self.history.values()),
                "edge_count":len(fit["edges"]) if fit else 0,
                "last_event":self.events[-1] if self.events else None,
                "last_rejection":self.last_rejection,"load_error":self.load_error,
                "world_alignment":"unregistered","navigation_authority":False,
                "uncertainty":"Model covariance and residuals are not calibrated RF accuracy bounds"}

    def save(self,now=None,force=False):
        now=self.clock() if now is None else now
        if not self.path or not self.dirty or (not force and now-self.last_saved<5):return
        self.path.parent.mkdir(parents=True,exist_ok=True)
        value={"schema":SCHEMA,"history":self.history,"last_seen":self.last_seen,"fit":self.fit,
               "previous":self.previous,"epoch":self.epoch,"recalibrating":self.recalibrating,
               "moved":self.moved,"events":self.events,"suspicions":self.suspicions}
        raw=json.dumps(value,allow_nan=False,separators=(",",":"))
        if self.load_error and self.path.exists() and not self.loaded:
            backup=self.path.with_suffix(".invalid.json")
            if not backup.exists():backup.write_bytes(self.path.read_bytes())
            self.loaded=True
        temporary=self.path.with_suffix(".tmp")
        with temporary.open("w") as stream:
            stream.write(raw);stream.flush();os.fsync(stream.fileno())
        os.replace(temporary,self.path);self.last_saved=now;self.dirty=False

def localize_relative(baseline, observations):
    """Rover spheres against the frozen baseline; never implies world alignment."""
    points={p["bssid"]:np.array([p["x_m"],p["y_m"],p["z_m"]]) for p in baseline["positions"]}
    observations=[r for r in observations if r["target_bssid"] in points and positive(r.get("distance_m"))]
    dimension=baseline["dimension"];needed=4 if dimension==3 else 3
    result={"valid":False,"position":None,"navigation_authority":False,
            "status":"awaiting_rover_ranges","motion_compensated":False,
            "moving_rover_requires_motion_compensation":True}
    if len(observations)<needed:return result
    anchors=np.array([points[r["target_bssid"]] for r in observations]);ranges=np.array([r["distance_m"] for r in observations])
    result["rover_range_time_span_s"]=max(r["observed"] for r in observations)-min(r["observed"] for r in observations)
    matrix=2*(anchors[1:]-anchors[0]);rhs=ranges[0]**2-ranges[1:]**2+(anchors[1:]**2).sum(axis=1)-(anchors[0]**2).sum()
    if np.linalg.matrix_rank(matrix,tol=1e-5)<dimension:
        result["status"]="unobservable_rover_geometry";return result
    initial=np.linalg.lstsq(matrix,rhs,rcond=None)[0]
    if dimension==2:
        z2=float(np.median(ranges**2-((anchors[:,:2]-initial[:2])**2).sum(axis=1)))
        if z2<-.25:
            result["status"]="inconsistent_3d_ranges";return result
        initial[2]=math.sqrt(max(z2,.001))
        lower=[-np.inf,-np.inf,0.];upper=[np.inf,np.inf,np.inf]
    else:lower=-np.inf;upper=np.inf
    fit=least_squares(lambda x:np.linalg.norm(x-anchors,axis=1)-ranges,initial,
                      loss="soft_l1",f_scale=.5,bounds=(lower,upper),max_nfev=100)
    residuals=np.linalg.norm(fit.x-anchors,axis=1)-ranges
    rms=float(np.sqrt(np.mean(residuals**2)));threshold=max(.75,.15*float(np.median(ranges)))
    result.update(residual_acceptance_m=threshold,uncertainty="Anchor baseline and FTM bias require physical calibration")
    if rms>threshold:
        result["status"]="inconsistent_rover_ranges";return result
    position={"x_m":float(fit.x[0]),"y_m":float(fit.x[1]),"z_m":float(fit.x[2]),
              "residual_rms_m":rms,"residuals_m":residuals.tolist(),"baseline_epoch":baseline["epoch"]}
    if dimension==2:
        result["status"]="mirror_ambiguous_3d"
        result["position_hypotheses"]=[position,dict(position,z_m=-position["z_m"])]
        result["ambiguity"]="Relative anchor plane does not determine the rover's side of that plane."
    else:
        result.update(valid=True,status="relative_position",position=position)
    return result
