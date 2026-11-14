import unittest
import numpy as np
from tof_display_fit import display_fits

class DisplayFitTests(unittest.TestCase):
    def test_insufficient_links_have_diagnostic_position_only(self):
        fixed={'a':[0,0,0],'b':[3,0,0]}
        edges=[dict(a=k,b='u',distance_m=2,sigma_m=.25) for k in fixed]
        row=display_fits(fixed,['a','b','u','unseen'],edges,fixed)[0]
        self.assertEqual(row['bssid'],'u')
        self.assertEqual(row['fit_quality'],'red')
        self.assertTrue(row['ambiguous'])
        self.assertFalse(row['navigation_authority'])
        self.assertLess(row['residual_rms_m'],.01)
    def test_joint_fit_with_known_geometry(self):
        fixed={'a':[0,0,0],'b':[5,0,0],'c':[0,5,0],'d':[0,0,5]}
        p=np.array([1.,2.,3.])
        edges=[dict(a=k,b='u',distance_m=np.linalg.norm(p-v),sigma_m=.25) for k,v in fixed.items()]
        v=display_fits(fixed,[*fixed,'u'],edges,dict(fixed,u=p),upstairs=['u'])[0]
        self.assertEqual(v['fit_quality'],'green');self.assertLess(v['residual_rms_m'],1e-6)
