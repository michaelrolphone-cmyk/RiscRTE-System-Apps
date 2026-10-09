"""Explicit shared QuickActions build selection; never expands runtime grants."""
import re
import shutil

def options(parser):
    parser.add_argument('--stage-logs',action='store_true',help='Automatic plain timestamped app/input/render diagnostic statements')
    parser.add_argument('--home-app',help='Explicit physical Home root .elf; independent of local Back')
    parser.add_argument('--quick-actions',action='store_true',help='Shared QuickActions, capability-selected Watch or static paper sheet; requires --alarm-client and namespace 1 read/write')
    parser.add_argument('--quick-radios',action='store_true',help='Explicit Wi-Fi/Bluetooth quick-control selection; requires --quick-actions and exact radio grants')

def configure(args,parser,root,output):
    if args.quick_radios and not args.quick_actions:parser.error('--quick-radios requires --quick-actions')
    if args.quick_actions and not args.alarm_client:parser.error('--quick-actions requires --alarm-client')
    flags=[]
    if getattr(args,'stage_logs',False):flags+=['-DPORTABLE_STAGE_LOGS']
    if args.home_app:
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*\.elf',args.home_app):parser.error('Invalid Home filename')
        flags+=['-DPORTABLE_HOME_APP="'+args.home_app+'"']
    if not args.quick_actions:return flags,[]
    flags+=['-DPORTABLE_QUICK_ACTIONS'];names=['quick_actions.c','quick_render.c','quick_session.c']
    if args.quick_radios:flags+=['-DPORTABLE_QUICK_RADIOS'];names+=['quick_radios.c']
    target=output/'licenses/quick-controls';target.mkdir(parents=True,exist_ok=True)
    for name in ['FAClassic-LICENSE.txt','Orbitron-OFL.txt','Rajdhani-OFL.txt','SOURCES.json']:
        shutil.copyfile(root/'lib/PortableApps/quick_fonts'/name,target/name)
    return flags,[root/'lib/PortableApps/src'/name for name in names]

def requirements(args,needs):
    if args.quick_actions:
        for name,version in [('storage.key-value',1),('rtc.clock',2),('board.battery',1)]:
            item={'capability':name,'api':version}
            if item not in needs:needs.append(item)
    if args.quick_radios:
        needs.extend({'capability':name,'api':1} for name in ['net.wifi','bluetooth.hci'])
