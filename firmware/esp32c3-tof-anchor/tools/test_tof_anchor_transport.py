import tempfile
import unittest
from pathlib import Path
from tof_anchor_transport import AnchorHTTPFallback
A="14:63:93:91:a4:31"
def packet(seq=1,mac=A):
    return {"schema":"rover.tof.anchor/1","bssid":mac,"ftm_responder":True,"boot_id":1,"seq":seq,"ranges":[]}
class TransportTests(unittest.TestCase):
    def test_accepts_only_expected_identity(self):
        with tempfile.TemporaryDirectory() as d:
            t=AnchorHTTPFallback(Path(d)/"r.json",fetch=lambda ip:packet())
            t.register(A,"192.168.1.68")
            self.assertTrue(t.poll_once(A,"192.168.1.68"))
            self.assertEqual(t.drain()[0][0]["ip"],"192.168.1.68")
            t.fetch=lambda ip:packet(mac="14:63:93:91:a4:32")
            self.assertFalse(t.poll_once(A,"192.168.1.68"));self.assertEqual(t.drain(),[])
    def test_results_are_latest_only(self):
        with tempfile.TemporaryDirectory() as d:
            t=AnchorHTTPFallback(Path(d)/"r.json",fetch=lambda ip:packet())
            t.register(A,"192.168.1.68");t.poll_once(A,"192.168.1.68")
            t.fetch=lambda ip:packet(seq=2);t.poll_once(A,"192.168.1.68")
            rows=t.drain();self.assertEqual(len(rows),1);self.assertEqual(rows[0][0]["seq"],2)
            self.assertEqual(t.drain(),[])
    def test_registry_restores_without_inventing_measurements(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/"r.json";t=AnchorHTTPFallback(p)
            t.register(A,"192.168.1.68");t=AnchorHTTPFallback(p)
            self.assertEqual(t.anchors,{A:"192.168.1.68"});self.assertEqual(t.drain(),[])
    def test_recent_udp_remains_preferred(self):
        with tempfile.TemporaryDirectory() as d:
            now=[100.]
            t=AnchorHTTPFallback(Path(d)/"r.json",clock=lambda:now[0])
            t.note_udp(packet(),"192.168.1.68",100)
            self.assertEqual(t.status()["anchors"][0]["transport"],"udp")
            now[0]=107
            self.assertEqual(t.status()["anchors"][0]["transport"],"http")
    def test_errors_are_explicit_and_not_fake_samples(self):
        with tempfile.TemporaryDirectory() as d:
            def fail(ip):raise OSError("unreachable")
            t=AnchorHTTPFallback(Path(d)/"r.json",fetch=fail);t.register(A,"192.168.1.68")
            self.assertFalse(t.poll_once(A,"192.168.1.68"));self.assertEqual(t.drain(),[])
            self.assertIn("unreachable",t.status()["anchors"][0]["error"])
    def test_non_lan_and_invalid_addresses_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            t=AnchorHTTPFallback(Path(d)/"r.json")
            for ip in ["127.0.0.1","0.0.0.0","224.0.0.1","file:///etc/passwd","::1"]:
                self.assertFalse(t.register(A,ip))
if __name__=="__main__":unittest.main()
