import os,subprocess,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class LastAttempt(unittest.TestCase):
 def test_normal_and_sanitized(self):
  with tempfile.TemporaryDirectory() as d:
   for sanitize in (False,True):
    exe=Path(d)/('san' if sanitize else 'normal')
    args=['cc','-std=c11','-Wall','-Wextra','-Werror','-DPORTABLE_LAST_ATTEMPT_CHECKPOINT','-DPORTABLE_NATIVE_CUSTODY_FENCE','-I'+str(ROOT/'lib/PortableApps/include'),'-I'+str(ROOT/'lib/PortableApps/src')]
    if sanitize:args+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
    subprocess.run([*args,str(ROOT/'tests/fixtures/last_attempt_checkpoint.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'})
if __name__=='__main__':unittest.main()
