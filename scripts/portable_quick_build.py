"""Explicit shared QuickActions build selection; never expands runtime grants."""
from pathlib import Path
import hashlib
import re
import shutil
import portable_idle_build

RESIDENT_CLIENT_VERSIONS={'springboard':'1.7.22','settings':'1.3.23','file_browser':'1.5.16','wifi_settings':'1.1.19',
    'ota_update':'1.2.9','app_store':'1.2.9','usb_sd_transfer':'0.1.4'}

def version(args,app,current):
    # Explicit X4 cohort reservations; ordinary/Watch profiles keep identity.
    if getattr(args,'resident_shell_host',False) and app=='paper_clock':
        if getattr(args,'display_settled_sdk',None):return '0.4.1'
        if getattr(args,'frontlight_tone',False):return '0.3.25'
        if getattr(args,'resident_policy',False):return '0.3.25'
        return '0.3.21' if getattr(args,'resident_loading_catalog',None) else '0.3.20'
    return RESIDENT_CLIENT_VERSIONS.get(app,current) if getattr(args,'resident_shell_client',False) else current

def options(parser):
    portable_idle_build.options(parser)
    group=parser.add_mutually_exclusive_group()
    group.add_argument('--resident-shell-host',action='store_true',help='Own shared X4 Quick Actions in the one resident default ELF')
    group.add_argument('--resident-shell-client',action='store_true',help='Use explicit resident checkpoints; never link Quick Actions UI')
    parser.add_argument('--resident-policy',action='store_true',help='Use explicit activity and settled host-owned power policy checkpoints')
    parser.add_argument('--resident-legacy-handoff',action='store_true',help='Host consumes explicit legacy HANDOFF and startup NO_PENDING continuation statuses')
    parser.add_argument('--resident-runtime-sdk',type=Path,help='Canonical Runtime app SDK with the frozen resident ABI')
    parser.add_argument('--stage-logs',action='store_true',help='Automatic plain timestamped app/input/render diagnostic statements')
    parser.add_argument('--home-app',help='Explicit physical Home root .elf; independent of local Back')
    parser.add_argument('--quick-actions',action='store_true',help='Shared QuickActions, capability-selected Watch or paper sheet; requires --alarm-client and namespace 1 read/write')
    parser.add_argument('--paper-transitions',action='store_true',help='Opt-in paper Quick Controls pull-down motion; independent of app crossfade')
    parser.add_argument('--quick-usb-transfer',action='store_true',help='Paper sheet launch to the separately granted USB SD transfer app')
    parser.add_argument('--quick-radios',action='store_true',help='Explicit Wi-Fi/Bluetooth quick-control selection; requires --quick-actions and exact radio grants')
    parser.add_argument('--frontlight-tone',action='store_true',help='Optional host-owned warm/cool slider through a size-checked display suffix; no new grants')
    parser.add_argument('--frontlight-tone-sdk',type=Path,help='SDK containing RiscDisplayOutputFrontlightV1.h')
    parser.add_argument('--display-settled-sdk',type=Path,help='Optional token-bound display settling SDK for the sleep overlay')

def configure(args,parser,root,output,includes=None):
    host=getattr(args,'resident_shell_host',False)
    client=getattr(args,'resident_shell_client',False)
    tone=getattr(args,'frontlight_tone',False)
    if tone and not (host and getattr(args,'paper_transitions',False)):
        parser.error('--frontlight-tone requires resident host and paper transitions')
    if tone!=bool(getattr(args,'frontlight_tone_sdk',None)):
        parser.error('--frontlight-tone and --frontlight-tone-sdk must be selected together')
    resident_flags=[]
    legacy=getattr(args,'resident_legacy_handoff',False)
    policy=getattr(args,'resident_policy',False)
    if policy and not (host or client):parser.error('--resident-policy requires a resident role')
    if policy and host and not portable_idle_build.selected(args):parser.error('Resident host policy requires the typed X4 idle helper')
    if getattr(args,'display_settled_sdk',None) and not (host and policy):parser.error('--display-settled-sdk requires a resident policy host')
    if legacy and not host:parser.error('--resident-legacy-handoff requires --resident-shell-host')
    if host or client:
        sdk=getattr(args,'resident_runtime_sdk',None)
        if not sdk or not includes or Path(includes).resolve()==(root/'lib/PortableApps/include').resolve():parser.error('Resident role requires --resident-runtime-sdk and a staged native SDK')
        if not args.alarm_client:parser.error('Resident role requires --alarm-client')
        if host and not args.quick_actions:parser.error('Resident host requires --quick-actions')
        if client and (args.quick_actions or args.quick_radios or getattr(args,'quick_usb_transfer',False) or getattr(args,'paper_transitions',False) or portable_idle_build.selected(args)):
            parser.error('Resident clients cannot contain Quick Actions, paper sheet transitions or an independent sleep helper')
        sdk_names=['RiscRuntimeV1.h','RiscResidentShellV1.h']
        if not (sdk/'RiscRuntimeV1.h').is_file():parser.error('Missing resident SDK header: RiscRuntimeV1.h')
        if 'RISC_RUNTIME_FAILURE_EVIDENCE_V1_SIZE' in (sdk/'RiscRuntimeV1.h').read_text():
            sdk_names.append('RiscFailureEvidenceV1.h')
        for name in sdk_names:
            source=sdk/name
            if not source.is_file():parser.error('Missing resident SDK header: '+name)
            shutil.copyfile(source,includes/name)
        if 'RISC_RUNTIME_RESIDENT_SHELL_V1_SIZE' not in (includes/'RiscRuntimeV1.h').read_text():parser.error('Runtime SDK lacks the resident getter')
        if host and not (includes/'RiscDisplayOutputSnapshotV1.h').is_file():parser.error('Resident host requires a completed-image display snapshot SDK')
        resident_flags=['-DPORTABLE_RESIDENT_SHELL_'+('HOST' if host else 'CLIENT'),'-DPORTABLE_ALARM_TERMINAL_RETENTION']
        if policy:
            contract=(includes/'RiscResidentShellV1.h').read_text()
            if 'RISC_RESIDENT_CHECKPOINT_POLICY' not in contract or 'RISC_RESIDENT_REPLY_POLICY_REQUEST' not in contract:
                parser.error('Resident policy requires the explicit Runtime policy handshake SDK')
            resident_flags+=['-DPORTABLE_RESIDENT_POLICY']
        if legacy:
            contract=(includes/'RiscResidentShellV1.h').read_text()
            if 'RISC_RESIDENT_HANDOFF=3' not in contract or 'RISC_RESIDENT_NO_PENDING=4' not in contract:
                parser.error('Legacy handoff requires the qualified Runtime status contract')
            resident_flags+=['-DPORTABLE_RESIDENT_LEGACY_HANDOFF']
        args.resident_shell_receipt={'api':1,'role':'host' if host else 'foreground','descriptor':'risc_resident_app_descriptor_v1',
            'runtime_sdk':str(sdk.resolve()),'sdk_sha256':{name:hashlib.sha256((includes/name).read_bytes()).hexdigest() for name in sdk_names},
            'failure_evidence':host and 'RiscFailureEvidenceV1.h' in sdk_names,'quick_renderer':host,'legacy_handoff':legacy,'host_power_policy':policy,'checkpoint':'explicit-settled-poll','child_home':'clean-return','hardware_verified':False,
            'source_sha256':{name:hashlib.sha256((root/name).read_bytes()).hexdigest() for name in (
                'lib/PortableApps/src/resident_adapter.inc','lib/PortableApps/src/resident_shell.inc','lib/PortableApps/src/resident_failure.inc','lib/PortableApps/src/failure_evidence.inc','lib/PortableApps/src/resident_policy.inc','lib/PortableApps/include/PortableResidentShell.h')}}
    if getattr(args,'quick_usb_transfer',False) and not args.quick_actions:parser.error('--quick-usb-transfer requires --quick-actions')
    if args.quick_radios and not args.quick_actions:parser.error('--quick-radios requires --quick-actions')
    if args.quick_actions and not args.alarm_client:parser.error('--quick-actions requires --alarm-client')
    flags=resident_flags
    if getattr(args,'display_settled_sdk',None):
        hashes={}
        for header in ('RiscDisplayOutputFrontlightV1.h','RiscDisplayOutputSettledV1.h'):
            source=args.display_settled_sdk/header
            if not source.is_file():parser.error('Missing optional settling SDK header: '+header)
            shutil.copyfile(source,includes/header);hashes[header]=hashlib.sha256(source.read_bytes()).hexdigest()
        flags+=['-DPORTABLE_DISPLAY_SETTLED']
        args.resident_shell_receipt['sleep_overlay_settling']={'optional':True,'token_bound':True,'timeout_ms':10000,'sdk_sha256':hashes}
    if tone:
        header='RiscDisplayOutputFrontlightV1.h';source=args.frontlight_tone_sdk/header
        if not source.is_file():parser.error('Missing optional frontlight SDK header: '+header)
        shutil.copyfile(source,includes/header)
        flags+=['-DPORTABLE_FRONTLIGHT_TONE']
        args.resident_shell_receipt['frontlight_tone']={'optional':True,'grants_added':False,
            'sdk_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
            'preference':'frontlight_tone','default_percent':50,'absent':'hide slider',
            'meaning':'dimensionless cool-to-warm ratio; no calibrated Kelvin or constant-luminance claim'}
    if getattr(args,'paper_transitions',False):
        if not args.quick_actions:parser.error('--paper-transitions requires --quick-actions')
        flags.append('-DPORTABLE_PAPER_TRANSITIONS')
    if getattr(args,'stage_logs',False):flags+=['-DPORTABLE_STAGE_LOGS']
    if args.home_app:
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*\.elf',args.home_app):parser.error('Invalid Home filename')
        flags+=['-DPORTABLE_HOME_APP="'+args.home_app+'"']
    if not args.quick_actions:return flags,[]
    if getattr(args,'quick_usb_transfer',False):flags+=['-DPORTABLE_QUICK_USB_TRANSFER']
    flags+=['-DPORTABLE_QUICK_ACTIONS'];names=['quick_actions.c','quick_render.c','quick_session.c']
    if args.quick_radios:flags+=['-DPORTABLE_QUICK_RADIOS'];names+=['quick_radios.c']
    target=output/'licenses/quick-controls';target.mkdir(parents=True,exist_ok=True)
    for name in ['FAClassic-LICENSE.txt','Orbitron-OFL.txt','Rajdhani-OFL.txt','SOURCES.json']:
        shutil.copyfile(root/'lib/PortableApps/quick_fonts'/name,target/name)
    if host:
        paths=['scripts/generate_quick_reference_assets.py','lib/PortableApps/src/quick_reference.inc',
            'test/native_apps/fixtures/quick-refined-reference.svg']
        paths += [str(path.relative_to(root)) for path in (root/'lib/PortableApps/quick_fonts').glob('reference*')]
        paths += ['lib/PortableApps/quick_fonts/REFERENCE.json']
        paths += ['lib/PortableApps/home_fonts/'+name for name in ('Orbitron-reference-700.ttf','Orbitron-reference-900.ttf','Rajdhani-600.ttf','Rajdhani-700.ttf')]
        args.resident_shell_receipt['quick_reference']={
            'source_sha256':{name:hashlib.sha256((root/name).read_bytes()).hexdigest() for name in paths},
            'audio':'admitted output modes; absent sound removes volume and silent controls',
            'clean_refresh':'display.output CLEAN_PRESENT flag and full CLEAN-intent submit',
            'low_power':'absent; no assigned manual low-power behavior or host capability contract',
            'usb':'matching two-column grid tile with or without sound; launches the existing transfer flow'}
        shutil.copyfile(root/'lib/PortableApps/quick_fonts/REFERENCE.json',target/'REFERENCE.json')
    idle_flags,idle_sources=portable_idle_build.configure(args,parser,root,output,includes)
    return flags+idle_flags,[root/'lib/PortableApps/src'/name for name in names]+idle_sources

def requirements(args,needs):
    portable_idle_build.requirements(args,needs)
    if args.quick_actions:
        for name,version in [('storage.key-value',1),('rtc.clock',2),('board.battery',1)]:
            item={'capability':name,'api':version}
            if item not in needs:needs.append(item)
    if args.quick_radios:
        needs.extend({'capability':name,'api':1} for name in ['net.wifi','bluetooth.hci'])


def exports(args, names):
    if getattr(args,'resident_shell_host',False) or getattr(args,'resident_shell_client',False):
        names.add('risc_resident_app_descriptor_v1')
    return names


def record(args, value):
    receipt=getattr(args,'resident_shell_receipt',None)
    if receipt:value['resident_shell']=receipt
