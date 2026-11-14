"""Display a joint range fit, including weak and rejected anchor solutions."""
import itertools
import numpy as np
from scipy.optimize import least_squares


def display_fits(fixed, active, edges, accepted, previous=None, upstairs=()):
    # Only range-connected nodes are placeable in the measured coordinate system.
    reachable = set(fixed)
    available = set(active) | set(fixed)
    links = [e for e in edges if e["a"] in available and e["b"] in available]
    while True:
        before = len(reachable)
        for e in links:
            if {e["a"], e["b"]} & reachable:
                reachable.update((e["a"], e["b"]))
        if len(reachable) == before:
            break
    nodes = sorted(reachable - set(fixed))
    if not nodes:
        return []
    ids = sorted(fixed) + nodes
    idx = {k:i for i,k in enumerate(ids)}
    links = [e for e in links if e["a"] in idx and e["b"] in idx and
             (e["a"] in nodes or e["b"] in nodes)]
    a = np.array([idx[e["a"]] for e in links])
    b = np.array([idx[e["b"]] for e in links])
    d = np.array([e["distance_m"] for e in links])
    sigma = np.maximum(.25, [e["sigma_m"] for e in links])
    limits = np.maximum(.75, np.maximum(.12*d, 2.5*sigma))
    bases = np.array([fixed[k] for k in sorted(fixed)])
    center = bases.mean(0)
    radius = max(float(np.median(d)), .25)
    def positions(x):
        return np.vstack((bases, x.reshape(-1,3)))
    def errors(x):
        p = positions(x)
        return np.linalg.norm(p[a]-p[b],axis=1)-d
    initial = []
    for k in nodes:
        value = (previous or {}).get(k, accepted.get(k))
        initial.append(np.array(value if value is not None else center,dtype=float))
    starts = [np.array(initial)]
    # Each sign pattern explores a different mirror solution for small real fleets.
    # For larger graphs use a fixed-size set of deterministic distributed seeds.
    signs = itertools.product((-1,1), repeat=len(nodes)) if len(nodes)<=3 else (
        np.where(np.sin(np.arange(len(nodes))*1.7+j)>0,1,-1) for j in range(8))
    for pattern in signs:
        starts.append(np.array([center + radius*np.array([.12*np.cos(i),.12*np.sin(i),z*.7])
                                for i,z in enumerate(pattern)]))
    bounds = np.full((len(nodes),3),-np.inf)
    # Upstairs is above the surveyed floor. It supplies no exact height.
    for i,k in enumerate(nodes):
        if k in upstairs:
            bounds[i,2]=0.
    fits = []
    for start in starts:
        result = least_squares(lambda x:errors(x)/sigma, np.maximum(start,bounds+1e-7).ravel(),
                               bounds=(bounds.ravel(),np.inf),loss="soft_l1",max_nfev=220)
        if np.isfinite(result.cost) and np.isfinite(result.x).all():
            fits.append(result)
    if not fits:
        return []
    best=min(fits,key=lambda f:f.cost)
    pts=positions(best.x);err=errors(best.x)
    jac=best.jac;_,s,vt=np.linalg.svd(jac,full_matrices=True)
    rank=int(np.count_nonzero(s>max(1e-6,s[0]*1e-5)))
    cov=np.linalg.pinv(jac.T@jac)
    out=[]
    for i,k in enumerate(nodes):
        mask=(a==idx[k])|(b==idx[k]); peers={e["b"] if e["a"]==k else e["a"] for e,m in zip(links,mask) if m}
        sl=slice(3*i,3*i+3);p=pts[idx[k]]
        null_space=rank<len(nodes)*3 and np.linalg.norm(vt[rank:,sl])>.05
        tolerance=float(np.median(limits[mask]))
        ambiguous=any(f.cost<=best.cost+.5 and np.linalg.norm(f.x[sl]-p)>tolerance for f in fits)
        uncertainty=float(np.sqrt(max(0,np.trace(cov[sl,sl]))))
        ratio=float(np.max(np.abs(err[mask])/limits[mask])); rms=float(np.sqrt(np.mean(err[mask]**2)))
        weak=len(peers)<4 or null_space or ambiguous
        immature=any(e.get("display_only") for e,m in zip(links,mask) if m)
        accepted_position=accepted.get(k)
        at_accepted=accepted_position is not None and np.linalg.norm(np.array(accepted_position)-p)<=tolerance
        major=weak or ratio>2 or uncertainty>2*tolerance
        quality="red" if major else "green" if at_accepted and not immature and ratio<=1 and uncertainty<=tolerance else "yellow"
        out.append(dict(bssid=k,x_m=float(p[0]),y_m=float(p[1]),z_m=float(p[2]),
            position_source="range_fit_display",fit_quality=quality,residual_rms_m=rms,
            max_residual_ratio=ratio,link_count=len(peers),model_sigma_m=None if null_space else uncertainty,
            ambiguous=bool(weak),qualified_position_available=accepted_position is not None,
            above_surveyed_floor=k in upstairs,navigation_authority=False))
    return out
