"""Gravity-direction observations from the surveyed rover's multi-zone LiDARs."""
from itertools import combinations

import numpy as np


def fit_floor(row, mount, camera_to_body):
    raw = row.get('raw') or row
    if raw.get('ready') is not True or raw.get('valid') is not True:
        return None
    distances = np.asarray(raw.get('distance_mm', []), dtype=float)
    status = np.asarray(raw.get('target_status', []))
    if distances.shape not in ((16,), (64,)) or status.shape != distances.shape:
        return None
    side = int(np.sqrt(distances.size))
    col, line = np.meshgrid(np.arange(side), np.arange(side))
    u, v = ((a.ravel()+.5)*2/side-1 for a in (col, line))
    u *= np.tan(np.deg2rad(mount['fov_horizontal_deg'])/2) * (-1 if mount.get('flip_x') else 1)
    v *= np.tan(np.deg2rad(mount['fov_vertical_deg'])/2) * (-1 if mount.get('flip_y') else 1)
    rays = np.column_stack((u, v, np.ones_like(u)))
    if mount.get('distance_semantics') != 'axial_z':
        rays /= np.linalg.norm(rays, axis=1)[:, None]
    transform = camera_to_body @ np.asarray(mount['T_front_camera_sensor_m'], dtype=float)
    rays = rays @ transform[:3,:3].T
    valid = (np.isin(status, mount['valid_statuses']) & np.isfinite(distances) &
             (distances > 0) & (rays[:,2] < .1))
    points = transform[:3,3] + rays[valid] * distances[valid,None] / 1000.
    if len(points) < 6:
        return None
    # Fit only a surface near the wheel-contact datum. A wall cannot supply up.
    tolerance = .012 + .015*np.linalg.norm(points-transform[:3,3], axis=1)
    if len(points) <= 12:
        triples = np.array(list(combinations(range(len(points)), 3)))
    else:
        triples = np.random.default_rng(0).integers(len(points), size=(240,3))
    a,b,c = points[triples].transpose(1,0,2)
    normals = np.cross(b-a, c-a)
    lengths = np.linalg.norm(normals, axis=1)
    keep = lengths > .0003
    normals, a = normals[keep]/lengths[keep,None], a[keep]
    normals *= np.where(normals[:,2] < 0, -1., 1.)[:,None]
    offsets = -np.sum(normals*a, axis=1)
    keep = (normals[:,2] > np.cos(np.deg2rad(38))) & (abs(offsets) < .065)
    if not np.any(keep):
        return None
    residuals = abs(normals[keep] @ points.T + offsets[keep,None])
    masks = residuals < tolerance
    counts = masks.sum(axis=1)
    best = np.argmax(counts - .1*np.minimum(residuals/tolerance, 1).mean(axis=1))
    inliers = points[masks[best]]
    if len(inliers) < 6:
        return None
    centroid = inliers.mean(axis=0)
    _, widths, basis = np.linalg.svd(inliers-centroid, full_matrices=False)
    n = basis[-1] if basis[-1,2] >= 0 else -basis[-1]
    height = -float(n @ centroid)
    if (widths[1]/np.sqrt(len(inliers)) < .022 or widths[2] > .26*widths[1] or
            n[2] < np.cos(np.deg2rad(38)) or abs(height) > .065):
        return None
    return dict(normal=n, inliers=len(inliers), residual_m=float(widths[2]/np.sqrt(len(inliers))),
                height_m=height, tilt_sigma_rad=float(np.arctan2(widths[2],np.sqrt(len(inliers))*widths[1])))
