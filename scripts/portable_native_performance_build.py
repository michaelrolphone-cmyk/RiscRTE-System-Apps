"""Default-off RAM trace selection without changing the selected native profile."""
import hashlib
import subprocess
from runtime_sdk_identity import verify_runtime_sdk
RUNTIME_COMMIT='273bdbccf65647a803ab0586a418f8283732baec'
DEFINE='-DPORTABLE_PERFORMANCE_TRACE'
def options(parser):
    parser.add_argument('--native-performance-trace',action='store_true',help='Explicit bounded RAM trace using the current native Runtime SDK; no legacy profile or serial stage-log substitution')
def selected(args):
    return getattr(args,'native_performance_trace',False)
def validate(args,parser,profile):
    if not selected(args):return
    if profile not in ('watch-native-time','native-time-touch'):
        parser.error('--native-performance-trace requires the selected Watch-native Springboard or native-time-touch Settings profile')
    if getattr(args,'performance_runtime_repo',None) or getattr(args,'performance_display_sdk',None) or getattr(args,'stage_logs',False):
        parser.error('--native-performance-trace cannot mix with legacy performance SDK selection or plain stage logs')
    if not getattr(args,'native_time_runtime_repo',None):
        parser.error('--native-performance-trace requires --native-time-runtime-repo')
def configure(args,parser,includes):
    if not selected(args):return [],None
    try:
        identity=verify_runtime_sdk(args.native_time_runtime_repo,RUNTIME_COMMIT)
        def read(name):return subprocess.check_output(['git','-C',str(args.native_time_runtime_repo),'show',RUNTIME_COMMIT+':sdk/app/'+name],stderr=subprocess.PIPE)
        runtime=read('RiscRuntimeV1.h');performance=read('RiscPerformanceV1.h')
        if (includes/'RiscRuntimeV1.h').read_bytes()!=runtime:
            parser.error('Selected native Runtime prefix differs from the exact trace SDK; refusing to replace profile/ABI')
        (includes/'RiscPerformanceV1.h').write_bytes(performance)
    except (OSError,ValueError,subprocess.CalledProcessError) as error:
        parser.error('Cannot stage current native RAM trace SDK: '+str(error))
    return [DEFINE],{'enabled':True,'mode':'bounded-owner-RAM','runtime_commit':RUNTIME_COMMIT,'runtime_sdk_identity':identity,'build_define':DEFINE,
        'sdk_sha256':{'RiscRuntimeV1.h':hashlib.sha256(runtime).hexdigest(),'RiscPerformanceV1.h':hashlib.sha256(performance).hexdigest()},
        'kernel_requires':'RISC_PERFORMANCE_TRACE=1','capture_command_ascii_hex':'706572660a','unsolicited_trace_output':False,
        'scope':'Software input acceptance/action, draw/raster, submit and observed completion; no physical input or photon timestamp',
        'profile_changed':False,'version_changed':False}
def record(root,receipt,record):
    if receipt:
        record['native_performance_trace']=receipt
        for path in ('scripts/portable_native_performance_build.py','scripts/runtime_sdk_identity.py'):
            record['source_sha256'][path]=hashlib.sha256((root/path).read_bytes()).hexdigest()
