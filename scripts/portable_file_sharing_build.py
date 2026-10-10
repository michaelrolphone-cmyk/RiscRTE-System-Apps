"""Explicit foreground authenticated HTTP Files profile. Default is disabled."""
import hashlib
from pathlib import Path
import shutil
import subprocess
VERSION = '1.5.19'
MODULES = ('WebDavCore','WebDavProperties','WebDavSession','WebDavDigest','WebDavSharing','PortableFileSharing')

def options(parser):
    parser.add_argument('--webdav-sharing',action='store_true',help='Explicit finite authenticated HTTP sharing of configured AppData exports')
    for name in ('export','tcp','entropy'):
        parser.add_argument('--sharing-'+name+'-instance',type=int,help='Explicit admitted '+name+' provider instance')

def validate(args,parser):
    values=[getattr(args,'sharing_'+name+'_instance',None) for name in ('export','tcp','entropy')]
    if not getattr(args,'webdav_sharing',False):
        if any(value is not None for value in values):parser.error('Sharing instances require --webdav-sharing')
        return
    if any(value is None or not 0<value<=0x7fffffff for value in values):
        parser.error('--webdav-sharing requires three explicit nonzero sharing instance IDs')
    if len(set(values))!=len(values):parser.error('Sharing providers require distinct instance IDs')

def configure(args,root,out,includes,cc):
    if not getattr(args,'webdav_sharing',False):return [],[],[]
    flags=['-DPORTABLE_FILE_SHARING','-DPORTABLE_NATIVE_CUSTODY_FENCE','-DPORTABLE_ALARM_TERMINAL_RETENTION']
    flags += [f'-DPORTABLE_FILE_SHARING_{name.upper()}_INSTANCE={getattr(args,"sharing_"+name+"_instance")}u' for name in ('export','tcp','entropy')]
    obj=out/'sharing-objects';obj.mkdir(parents=True,exist_ok=True)
    common=['-Os','-fPIC','-mtext-section-literals','-mlongcalls','-fvisibility=hidden','-ffreestanding','-fno-builtin','-Wall','-Wextra','-Werror','-I'+str(includes)]
    objects=[]
    for name in MODULES:
        target=obj/(name+'.o')
        subprocess.run([cc.removesuffix('gcc')+'g++','-std=c++17','-fno-exceptions','-fno-rtti',*common,'-c',str(root/'lib/RemoteFiles'/(name+'.cpp')),'-o',str(target)],check=True)
        objects.append(target)
    target=obj/'sha2.o'
    subprocess.run([cc,'-std=c11',*common,'-c',str(root/'lib/RemoteFiles/vendor/tinydtls_sha2/sha2.c'),'-o',str(target)],check=True)
    objects.append(target)
    files=[root/'Apps/file_browser_sharing.inc',root/'scripts/portable_file_sharing_build.py']
    files += [p for p in (root/'lib/RemoteFiles').rglob('*') if p.is_file()]
    return flags,[root/'lib/PortableApps/src/PortableNetworkSession.c',*objects],files

def requirements(args,needs):
    if not getattr(args,'webdav_sharing',False):return
    for name in ('storage.app-data.export','network.tcp.listener','crypto.entropy','net.wifi','storage.key-value'):
        item={'capability':name,'api':1}
        if item not in needs:needs.append(item)

def record(args,root,out,manifest,record):
    if not getattr(args,'webdav_sharing',False):return
    instances={name:getattr(args,'sharing_'+name+'_instance') for name in ('export','tcp','entropy')}
    bindings=record.setdefault('grant_bindings',{})
    expected={'storage.app-data.export':[instances['export']], 'network.tcp.listener':[instances['tcp']], 'crypto.entropy':[instances['entropy']], 'net.wifi':[15], 'storage.key-value':[1,6]}
    for name,values in expected.items():
        existing=bindings.get(name,[])
        # Root selection is deliberate. A browser-selected export must be the
        # same provider, which is closed before the sharing controller starts.
        if name=='storage.app-data.export' and args.storage_capability==name and args.storage_instance!=instances['export']:
            raise ValueError('Browser export and sharing export must select the same explicit provider')
        bindings[name]=list(dict.fromkeys([*existing,*values]))
    bindings.setdefault(args.storage_capability,[args.storage_instance])
    if args.secondary_storage_instance is not None:
        name=getattr(args,'secondary_storage_capability','storage.volume')
        bindings[name]=list(dict.fromkeys([*bindings.get(name,[]),args.secondary_storage_instance]))
    grants=[dict(item,instance_id=instance) for item in manifest['requires'] for instance in bindings.get(item['capability'],[0])]
    if len(grants)>16:raise ValueError(f'Sharing profile exceeds 16 live app grants: {len(grants)}')
    record.update(required_grants=grants,requested_capabilities=manifest['requires'],grant_count=len(grants))
    record['file_sharing']={'enabled':True,'transport':'authenticated-http','content_encrypted':False,'explicit_start':True,'lifetime_ms':300000,'listen_port':8080,'saved_profile':0,'credentials_namespace':6,'radio_policy_namespace':1,'instances':instances,'root_policy':'only boot-configured storage.app-data.export map','ble_setup':False,'hardware_qualified':False}
    record['source_sha256']['scripts/portable_file_sharing_build.py']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    dest=out/'licenses/webdav';dest.mkdir(parents=True,exist_ok=True)
    for name in ('lib/RemoteFiles/vendor/tinydtls_sha2/LICENSE.txt','lib/RemoteFiles/vendor/tinydtls_sha2/PROVENANCE.md'):
        source=root/name
        if source.is_file():shutil.copyfile(source,dest/source.name)
