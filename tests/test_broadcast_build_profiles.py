"""Telemetry is explicitly selected, versioned and bounded before packaging."""
import argparse,contextlib,io,os,subprocess,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
import portable_broadcast_build as broadcast
class BroadcastBuildProfiles(unittest.TestCase):
 def test_legacy_selection_rejected_without_output(self):
  for name in ['settings','springboard','file_browser','wifi','updates']:
   with self.subTest(app=name),tempfile.TemporaryDirectory() as temp:
    out=Path(temp)/'output';result=subprocess.run([sys.executable,ROOT/'scripts'/('build_portable_'+name+'.py'),'--ble-broadcast','--output-dir',out],capture_output=True,text=True,env=dict(os.environ,NATIVE_APP_CC='/must-not-compile'))
    self.assertEqual(result.returncode,2,result.stderr);self.assertIn('--ble-broadcast requires',result.stderr);self.assertFalse(out.exists())
 def test_flag_off_preserves_profile(self):
  args=argparse.Namespace();needs=[{'capability':'alarm.service','api':1}];before=list(needs);broadcast.requirements(args,needs);self.assertEqual(needs,before);self.assertEqual(broadcast.flags(args),[])
  for name in broadcast.VERSIONS:self.assertEqual(broadcast.version(args,name,'0.0.1'),'0.0.1')
 def test_versions_and_authority(self):
  args=argparse.Namespace(ble_broadcast=True)
  self.assertEqual(broadcast.VERSIONS,{'settings':'1.3.15','springboard':'1.7.7','file_browser':'1.5.10','wifi_settings':'1.1.12','ota_update':'1.2.2','app_store':'1.2.2'})
  for count in [2,15,16]:
   needs=[{'capability':'storage.key-value','api':1},{'capability':'alarm.service','api':2}]+[{'capability':'test.'+str(i),'api':1} for i in range(count-2)]
   if count==16:
    with self.assertRaises(ValueError):broadcast.requirements(args,needs)
   else:broadcast.requirements(args,needs);self.assertEqual(needs.count(broadcast.CAPABILITY),1)
  for needs in [[],[dict(broadcast.CAPABILITY)],[{'capability':'storage.key-value','api':1},{'capability':'alarm.service','api':1}]]:
   with self.assertRaises(ValueError):broadcast.requirements(args,needs)
 def test_idle_versions_compose_with_telemetry_custody(self):
  args=argparse.Namespace(ble_broadcast=True,x4_idle_source=Path('/explicit/idle.c'))
  for name in broadcast.VERSIONS:
   version=broadcast.portable_idle_build.VERSIONS[name]
   needs=[{'capability':'storage.key-value','api':1},{'capability':'alarm.service','api':2},dict(broadcast.CAPABILITY)]
   record={'required_grants':[dict(r,instance_id=1 if r['capability']=='storage.key-value' else 0) for r in needs]}
   manifest={'id':name,'version':version,'requires':needs}
   broadcast.record(args,ROOT,record,manifest,broadcast.DEFINES+['-DPORTABLE_NATIVE_CUSTODY_FENCE','-DALARM_SERVICE_TAGGED_V2'])
   self.assertEqual(record['ble_broadcast']['instance_id'],0)
   with self.assertRaises(ValueError):broadcast.record(args,ROOT,record,{**manifest,'version':'0.0.1'},record['build_defines'])

if __name__=='__main__':unittest.main()
