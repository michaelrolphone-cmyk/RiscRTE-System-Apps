import unittest
from tof_coordinator import RoverRequest

class RequestReconciliationTests(unittest.TestCase):
    def test_completed_target_releases_pending(self):
        r=RoverRequest();r.submitted("AA:00:00:00:00:01",1)
        r.observe({"ftm":{"target":"aa:00:00:00:00:01","status":"success"}})
        self.assertIsNone(r.pending)

    def test_reboot_invalidates_old_request(self):
        r=RoverRequest();r.observe({"boot_id":"old","uptime_ms":1000,"ftm":{}})
        r.submitted("aa:00:00:00:00:01",1)
        self.assertTrue(r.observe({"boot_id":"new","uptime_ms":20,
            "ftm":{"status":"never_requested","target":None,"busy":False,"retiring":False}}))
        self.assertIsNone(r.pending)

    def test_authoritative_empty_state_reconciles_initial_boot(self):
        r=RoverRequest();r.submitted("aa:00:00:00:00:01",1)
        r.observe({"ftm":{"status":"never_requested","target":None}})
        self.assertIsNone(r.pending)

    def test_other_consumer_busy_or_retiring_is_preserved(self):
        for state in [{"busy":True},{"retiring":True},{"status":"success"}]:
            r=RoverRequest();r.submitted("aa:00:00:00:00:01",1)
            r.observe({"ftm":dict(state,target="aa:00:00:00:00:02")})
            self.assertIsNotNone(r.pending)

    def test_uptime_rollback_without_boot_id(self):
        r=RoverRequest();r.observe({"uptime_ms":1000});r.submitted("a",1)
        self.assertTrue(r.observe({"uptime_ms":20}))
        self.assertIsNone(r.pending)

if __name__=="__main__":unittest.main()
