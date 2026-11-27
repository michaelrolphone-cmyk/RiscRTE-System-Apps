"""Actual Runtime checkpoint admission/retention with production diagnostic latch."""
import argparse,os,subprocess,tempfile
from pathlib import Path
S=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--runtime',required=True,type=Path);p.add_argument('--sanitize',action='store_true');a=p.parse_args();r=a.runtime.resolve()
 flags=['-g','-Wall','-Wextra','-Werror'];san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'] if a.sanitize else []
 env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'}
 with tempfile.TemporaryDirectory(prefix='last-attempt-runtime-') as d:
  d=Path(d)
  subprocess.run(['cc','-std=c11',*flags,*san,'-fPIC','-fvisibility=hidden','-shared','-DPORTABLE_LAST_ATTEMPT_CHECKPOINT','-DPORTABLE_NATIVE_CUSTODY_FENCE','-I'+str(S/'lib/PortableApps/include'),'-I'+str(S/'lib/PortableApps/src'),str(S/'tests/fixtures/last_attempt_runtime_app.c'),'-o',str(d/'default.elf')],check=True)
  sources=['src/bootstrap/Json.cpp','src/bootstrap/Board.cpp','src/bootstrap/Runtime.cpp','src/runtime/streams/AppStreamSessions.cpp','src/runtime/streams/ProviderQueueHost.cpp','src/runtime/drivers/ProviderGraphV2.cpp','src/runtime/drivers/ProviderModuleV2.cpp']
  inc=['src','sdk/app','sdk/driver','sdk/hardware','lib/ArduinoJson/src','test/drivers/stubs','test']
  subprocess.run(['c++','-std=c++17',*flags,*san,'-fno-pie','-no-pie','-Wno-missing-field-initializers','-rdynamic',*['-I'+str(r/i) for i in inc],*[str(r/i) for i in sources],str(S/'tests/fixtures/last_attempt_runtime.cpp'),'-ldl','-o',str(d/'test')],check=True)
  for mode in ['normal','off','retained']:subprocess.run([str(d/'test'),str(d),mode],check=True,env=env)

if __name__=="__main__":main()
