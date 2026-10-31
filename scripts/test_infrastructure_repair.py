#!/usr/bin/env python3
import copy,json,os,pathlib,shutil,tempfile,unittest
from inspect_infrastructure_state import read_snapshot,summary
from repair_infrastructure import migrate
from provision_infrastructure import seed
ROOT=pathlib.Path(__file__).resolve().parents[1]
RUNTIME=pathlib.Path(os.environ['RISC_RUNTIME_ROOT'])
class Repair(unittest.TestCase):
 def test_private_state_and_pending_survive_policy_migration(self):
  policy=json.loads((ROOT/'profiles/esp32s3-infrastructure/fixtures/node-policy.example.json').read_text());old=copy.deepcopy(policy)
  for provider in old['service']['providers']:provider.update(revision=6,package_digest='e8e06f34552ecf0df59b3b871310ccf7c842e5e67b3ede755462d065c3c330ac')
  state=json.loads((ROOT/'profiles/esp32s3-infrastructure/fixtures/node-state.example.json').read_text());state.update(participant_id='participant-test',credentials={'private':'never-print'},setup_policy=copy.deepcopy(old),relay={'identity':{'private_key':'sealed-enrollment-target-key'},'enrollments':[{'payload':{'sealed':{'ciphertext':'unchanged-ciphertext'}}}]},pending=[{'action':15,'request':{'operation_id':'original-pending','family':'observation','operation':'publish','arguments':{'private':'never-print'}}}])
  migrated_policy,migrated_state=migrate(old,state,policy);self.assertEqual(state['pending'],migrated_state['pending']);self.assertEqual(state['relay'],migrated_state['relay']);self.assertEqual(state['credentials'],migrated_state['credentials']);self.assertNotIn('never-print',json.dumps(summary(state,16)))
  with tempfile.TemporaryDirectory() as folder:
   root=pathlib.Path(folder);bundle=root/'bundle';(bundle/'tools').mkdir(parents=True);(bundle/'scripts').mkdir();(bundle/'tools/littlefs').symlink_to(RUNTIME.parent/'native-build/esp-idf/risc_littlefs/pinned/littlefs');shutil.copy2(ROOT/'scripts/infrastructure_read.c',bundle/'scripts/infrastructure_read.c')
   original=root/'original.bin';seed(RUNTIME,old,state,original,sequence=16)
   flash=bytearray(b'\xff'*16777216);flash[0x270000:0x2f0000]=original.read_bytes();self.assertEqual(read_snapshot(bundle,flash),(old,state,16))
   repaired=root/'repaired.bin';seed(RUNTIME,migrated_policy,migrated_state,repaired,original,sequence=17);flash[0x270000:0x2f0000]=repaired.read_bytes();self.assertEqual(read_snapshot(bundle,flash),(migrated_policy,migrated_state,17));self.assertEqual(original.stat().st_size,524288)
  unknown=copy.deepcopy(old);unknown['service']['providers'][0]['package_digest']='f'*64
  with self.assertRaises(ValueError):migrate(unknown,state,policy)
if __name__=='__main__':unittest.main()
