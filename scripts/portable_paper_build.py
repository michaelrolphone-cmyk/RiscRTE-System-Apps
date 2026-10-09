"""Explicit paper crossfade SDK staging for Clock and Springboard only."""
import hashlib
import shutil
from pathlib import Path

HEADERS=('RiscDisplayOutputV1.h','RiscDisplayOutputPowerV1.h',
         'RiscDisplayOutputMetricsV1.h','RiscDisplayOutputSnapshotV1.h')
SOURCES=('Apps/PaperFrame.h','lib/PortableApps/include/PortablePaperTransition.h',
         'lib/PortableApps/src/paper_transition.inc','scripts/portable_paper_build.py')
# Allocated for the explicitly selected paper motion profile, not normal builds.
VERSIONS={'paper_clock':'0.3.9','springboard':'1.7.6'}

def motion_receipt(root):
    paths=('lib/PortableApps/src/quick_actions.c','lib/PortableApps/src/quick_adapter.inc',
           'lib/PortableApps/src/quick_paper.inc','lib/PortableApps/include/PortableQuickActions.h',
           'scripts/portable_quick_build.py')
    return {'enabled':True,'build_define':'-DPORTABLE_PAPER_TRANSITIONS',
            'source_sha256':{path:hashlib.sha256((root/path).read_bytes()).hexdigest() for path in paths}}

def options(parser):
    parser.add_argument('--paper-crossfade',action='store_true',
                        help='Opt-in completed-image MONO1 app crossfade')
    parser.add_argument('--paper-display-sdk',type=Path,
                        help='Canonical display SDK directory; all four exact prefix/snapshot headers required')

def validate(args,parser):
    if bool(args.paper_crossfade)!=bool(args.paper_display_sdk):
        parser.error('--paper-crossfade requires --paper-display-sdk and vice versa')

def stage(args,parser,root,out,base):
    if not args.paper_crossfade:return base,[],None
    try:
        source={name:(args.paper_display_sdk/name).read_bytes() for name in HEADERS}
    except OSError as error:
        parser.error('Cannot read canonical paper display SDK (all four headers required): '+str(error))
    # Keep quoted prefix includes together. Never modify the normal source SDK
    # or assume that a directory name proves a provider version or commit.
    directory=out/'paper-sdk'
    if directory.exists():shutil.rmtree(directory)
    includes=directory/'include'
    shutil.copytree(base,includes)
    shutil.copytree(root/'lib/PortableApps/time',directory/'time')
    for name,data in source.items():(includes/name).write_bytes(data)
    receipt={'enabled':True,'build_define':'-DPORTABLE_PAPER_CROSSFADE',
             'source_directory':str(args.paper_display_sdk.resolve()),
             'compiled_include_directory':str(includes.resolve()),
             'sdk_sha256':{name:hashlib.sha256(data).hexdigest() for name,data in source.items()},
             'source_sha256':{path:hashlib.sha256((root/path).read_bytes()).hexdigest() for path in SOURCES},
             'duration_ms':400,'mode':'ordered-4x4-MONO1-completed-snapshot',
             'allocation_bytes_at_800x480':48000,'fallback':'immediate incoming scene'}
    return includes,['-DPORTABLE_PAPER_CROSSFADE'],receipt
