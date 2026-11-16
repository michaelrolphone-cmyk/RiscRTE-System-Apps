import json
import unittest
from tof_anchor_packets import AnchorReports


class AnchorPacketTest(unittest.TestCase):
    def message(self, sequence):
        return json.dumps(dict(schema="rover.tof.anchor/1",bssid="registered",boot_id=1,seq=sequence,ranges=["a"*1750]),separators=(",",":")).encode()

    def test_isolation_duplicates_expiry_and_complete_datagrams(self):
        reader=AnchorReports()
        a,b=self.message(7),self.message(8)
        self.assertIsNone(reader.receive("left",a[:1460],1.0))
        self.assertIsNone(reader.receive("right",b[:1460],1.01))
        self.assertEqual(reader.receive("left",a[1460:],1.05)["seq"],7)
        self.assertEqual(reader.receive("right",b[1460:],1.06)["seq"],8)
        self.assertIsNone(reader.receive("left",a,1.1))
        self.assertIsNone(reader.receive("left",b[:1460],2.0))
        self.assertIsNone(reader.receive("left",b[1460:],4.0))
        self.assertEqual(reader.receive("left",b,4.1)["seq"],8)
