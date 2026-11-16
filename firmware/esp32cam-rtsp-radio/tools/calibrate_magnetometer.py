#!/usr/bin/env python3
"""Fit hard/soft-iron correction from measured 3D rotations; never infer it from one pose."""
import argparse
import json
from pathlib import Path
import numpy as np


def fit(samples):
    raw = np.asarray(samples, dtype=float)
    if raw.ndim != 2 or raw.shape[1] != 3 or len(raw) < 60 or not np.isfinite(raw).all():
        raise ValueError('Need at least60 finite three-axis magnetic samples from diverse orientations')
    origin = np.median(raw, axis=0)
    scale = np.linalg.norm(np.std(raw, axis=0))
    if scale < 1:
        raise ValueError('Insufficient magnetic variation; rotate through all three axes')
    points = (raw - origin) / scale
    keep = np.ones(len(points), dtype=bool)
    for _ in range(4):
        x, y, z = points[keep].T
        design = np.column_stack((x*x, y*y, z*z, 2*x*y, 2*x*z, 2*y*z, x, y, z))
        coefficients, _, rank, singular = np.linalg.lstsq(design, np.ones(len(x)), rcond=None)
        if rank < 9 or singular[-1] / singular[0] < 1e-4:
            raise ValueError('Rotation coverage is insufficient for a full3D correction')
        a, b, c, d, e, f, g, h, i = coefficients
        quadratic = np.array([[a, d, e], [d, b, f], [e, f, c]])
        center = -.5 * np.linalg.solve(quadratic, [g, h, i])
        normalized = quadratic / (1 + center @ quadratic @ center)
        eigenvalues, vectors = np.linalg.eigh(normalized)
        if eigenvalues.min() <= 0 or eigenvalues.max() / eigenvalues.min() > 100:
            raise ValueError('Measurements do not form a usable magnetic ellipsoid')
        correction = (vectors * np.sqrt(eigenvalues)) @ vectors.T / scale
        bias = origin + center * scale
        radii = np.linalg.norm((raw - bias) @ correction.T, axis=1)
        residual = np.abs(radii - 1)
        cutoff = max(.03, float(np.median(residual) + 4*np.median(np.abs(residual-np.median(residual)))))
        candidate = residual < cutoff
        if candidate.sum() < 60:
            raise ValueError('Too few consistent magnetic observations')
        if np.array_equal(candidate, keep):
            break
        keep = candidate
    corrected = (raw[keep] - bias) @ correction.T
    lengths = np.linalg.norm(corrected, axis=1)
    unit = corrected / lengths[:, None]
    coverage = np.linalg.eigvalsh(unit.T @ unit / len(unit))
    residual_rms = float(np.sqrt(np.mean((lengths-1)**2)))
    if coverage.min() < .08 or residual_rms > .08:
        raise ValueError('Poor3D coverage or inconsistent field; calibration not accepted')
    # Heading needs shape and bias; preserve the observed field scale with unit determinant.
    correction /= np.linalg.det(correction)**(1/3)
    return {'schema': 'rover.ahrs.calibration/1', 'mag_bias_uT': bias.tolist(),
            'mag_correction': correction.tolist(), 'mag_calibrated': True}, {
                'samples': len(raw), 'accepted_samples': int(keep.sum()),
                'relative_residual_rms': residual_rms, 'direction_coverage_eigenvalues': coverage.tolist()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('samples', type=Path, help='JSON lines containing field_uT vectors or telemetry envelopes')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    values = []
    for line in args.samples.read_text().splitlines():
        value = json.loads(line)
        if isinstance(value, list):
            values.append(value)
        else:
            sensor = value.get('ahrs', value)
            sensor = sensor.get('mag', sensor)
            if sensor.get('valid') is True and sensor.get('age_ms', 10000) < 1000:
                values.append(sensor['field_uT'])
    profile, quality = fit(values)
    args.output.write_text(json.dumps(profile, indent=2) + '\n')
    print(json.dumps(quality))


if __name__ == '__main__':
    main()
