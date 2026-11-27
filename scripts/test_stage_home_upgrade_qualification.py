#!/usr/bin/env python3
import io,unittest,zipfile
from stage_home_upgrade_qualification import verify_archive,MARKERS
class Proofs(unittest.TestCase):
 def archive(self,logs=True,boot=False):
  output=io.BytesIO()
  with zipfile.ZipFile(output,'w') as z:
   if logs:
    for name,markers in MARKERS.items():z.writestr(name,'\n'.join(markers))
   if boot:z.writestr('boot/bootloader.bin',b'not the qualified bootloader')
  return output.getvalue()
 def test_missing_logs_rejected(self):
  with self.assertRaises(ValueError):verify_archive(self.archive(False))
 def test_log_text_alone_is_not_bootloader_proof(self):
  with self.assertRaises(ValueError):verify_archive(self.archive())
 def test_wrong_bootloader_rejected(self):
  with self.assertRaises(ValueError):verify_archive(self.archive(boot=True))
if __name__=='__main__':unittest.main()
