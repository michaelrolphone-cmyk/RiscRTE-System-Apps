import importlib.util
from pathlib import Path
import unittest
import numpy as np
path = Path(__file__).resolve().parents[2] / 'tools/calibrate_magnetometer.py'
spec = importlib.util.spec_from_file_location('calibration', path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class MagneticFit(unittest.TestCase):
    def test_recovers_bias_and_full_soft_iron_shape(self):
        rng = np.random.default_rng(5)
        unit = rng.normal(size=(800, 3))
        unit /= np.linalg.norm(unit, axis=1)[:, None]
        matrix = np.array([[1.4, .2, .1], [.2, .8, .05], [.1, .05, 1.1]])
        bias = np.array([60, -30, 120])
        samples = unit @ np.linalg.inv(matrix).T * 48 + bias + rng.normal(scale=.05, size=unit.shape)
        profile, quality = module.fit(samples)
        self.assertLess(np.linalg.norm(np.array(profile['mag_bias_uT'])-bias), .05)
        lengths = np.linalg.norm((samples - profile['mag_bias_uT']) @ np.array(profile['mag_correction']).T, axis=1)
        self.assertLess(np.std(lengths) / np.mean(lengths), .004)
        self.assertTrue(profile['mag_calibrated'])

    def test_single_pose_or_planar_rotation_is_not_calibration(self):
        for values in (np.repeat([[20, 30, 40]], 100, axis=0),
                       np.array([[30*np.cos(t), 30*np.sin(t), 40] for t in np.linspace(0, 6.28, 100)])):
            with self.assertRaises(ValueError):
                module.fit(values)

if __name__ == '__main__':
    unittest.main()
