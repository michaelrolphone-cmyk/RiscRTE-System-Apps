"""Pinned inputs and preservation checks for offline shared Wi-Fi candidates."""
import hashlib,json,subprocess,sys,zipfile
from pathlib import Path,PurePosixPath
RUNTIME='532c502fb2aad358d46166d5fd1df8358148f20e'
SYSTEM='710b200021b983ea55eb86a6fbddbbf5ce10a984'
BOOTFS='ab9ff33975680c5bb8d9a2e7ed346d0300732a4e'
LANES=('watch','wave','x4')
ADDITIONS=(('ui.scene',1),)
def check(ok,message):
    if not ok:raise ValueError(message)
def sha(b):return hashlib.sha256(b).hexdigest()
def encode(value):return (json.dumps(value,sort_keys=True,indent=2)+'\n').encode()
def elf_receipt(record,elf,name):
    check(record.get('file')==name and type(record.get('size_bytes')) is int and record['size_bytes']==len(elf) and record.get('sha256')==sha(elf),'ELF build receipt differs: '+name)
def safe(name):
    check(isinstance(name,str) and bool(name),'Invalid path')
    p=PurePosixPath(name)
    check(not p.is_absolute() and '..' not in p.parts and '\\' not in name and str(p)==name,'Unsafe path '+name)
    return name
def pin(root,expected):
    check(subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()==expected,'Wrong source '+str(root))
    subprocess.run(['git','-C',str(root),'diff','--exit-code','HEAD','--'],check=True,capture_output=True)
def tree(root):
    check(root.is_dir(),'Missing tree '+str(root))
    check(not any(p.is_symlink() for p in root.rglob('*')),'Symlink in tree')
    return {safe(p.relative_to(root).as_posix()):p.read_bytes() for p in root.rglob('*') if p.is_file()}
def write(root,entries):
    root.mkdir(parents=True,exist_ok=False)
    for n,b in entries.items():
        p=root/safe(n);p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b)
def unzip(path):
    with zipfile.ZipFile(path) as z:
        names=z.namelist();check(len(set(names))==len(names),'Duplicate archive paths')
        for n in names:safe(n.rstrip('/'))
        return {n:z.read(n) for n in names if not n.endswith('/')}
def zip_roundtrip(path,entries):
    with zipfile.ZipFile(path,'w',compression=zipfile.ZIP_DEFLATED) as z:
        for n,b in sorted(entries.items()):
            i=zipfile.ZipInfo(safe(n),(1980,1,1,0,0,0));i.compress_type=zipfile.ZIP_DEFLATED;i.external_attr=0o100644<<16;z.writestr(i,b)
    check(unzip(path)==entries,'SD roundtrip differs')
def selected(entries):
    boot=json.loads(entries['boot.json']);rows=[]
    for i,r in enumerate(boot['app_capabilities']):
        m=json.loads(entries[r['manifest']])
        if m.get('id')=='wifi_settings':rows.append((i,r,m))
    check(len(rows)==1,'Expected one Wi-Fi selection')
    return boot,*rows[0]
def load(args):
    pin(args.system,SYSTEM);pin(args.bootfs,BOOTFS)
    sys.path[:0]=[str(args.system/'scripts'),str(args.bootfs/'scripts')]
    from current_bootfs import build
    from read_only_spiffs import read_image
    from image_envelope import MAGIC,decode_image,encode_image
    lock=json.loads(args.baseline_lock.read_text())
    names={f'product-{l}-{s}' for l in LANES for s in ('store.bin','sd-apps.zip')}
    check(len(lock['assets'])==6 and {r['name'] for r in lock['assets']}==names,'Wrong baseline asset set')
    for r in lock['assets']:
        b=(args.baseline/safe(r['name'])).read_bytes()
        check(len(b)==r['bytes'] and sha(b)==r['sha256'],'Baseline identity differs '+r['name'])
    elf=(args.shared/'wifi_settings.elf').read_bytes();manifest=(args.shared/'manifest.json').read_bytes()
    record=json.loads((args.shared/'build.json').read_text());m=json.loads(manifest)
    elf_receipt(record,elf,'wifi_settings.elf')
    check(sha(manifest)==record['manifest_sha256'] and record['source_pins']['runtime']==RUNTIME,'Build manifest/runtime differs')
    check(m.get('id')=='wifi_settings' and m.get('file_name')=='wifi_settings.elf','Wrong shared app')
    check(record.get('source_pins',{}).get('system')==subprocess.check_output(['git','-C',str(Path(__file__).resolve().parents[1]),'rev-parse','HEAD'],text=True).strip(),'Candidate source receipt differs')
    check(record.get('working_tree_dirty') is False,'Dirty candidate build')
    check(m.get('runtime_lifecycle')=='foreground','Shared lifecycle missing')
    decode=lambda b:decode_image(b) if b.startswith(MAGIC) else b
    lanes={}
    for lane in LANES:
        raw=(args.baseline/f'product-{lane}-store.bin').read_bytes();stored=read_image(raw,len(raw))
        lanes[lane]=(len(raw),stored,{n:decode(b) for n,b in stored.items()},unzip(args.baseline/f'product-{lane}-sd-apps.zip'))
    return elf,manifest,lanes,build,read_image,MAGIC,decode,encode_image
def grants(before,row,manifest):
    # Never infer a feature migration from a smaller declaration list.
    declared={(r['capability'],r['api']) for r in manifest.get('requires',[])+manifest.get('optional',[])}
    result=list(row['grants'])
    check(all((g['capability'],g['api']) in declared for g in result),'Legacy grant no longer declared; parity migration required')
    boot=json.loads(before['boot.json'])
    for cap,api in ADDITIONS:
        check((cap,api) in declared,'Shared capability undeclared')
        owners=[d for d in boot['drivers'] if any(p.get('capability')==cap and p.get('api')==api for p in json.loads(before[d['manifest']]).get('provides',[]))]
        check(len(owners)==1,'Missing or ambiguous provider '+cap)
        want={'capability':cap,'api':api,'instance_id':owners[0].get('instance_id',0)}
        matches=[g for g in result if (g['capability'],g['api'])==(cap,api)]
        if matches:check(matches==[want],'Shared grant changed')
        else:result.append(want)
    for r in manifest.get('requires',[]):
        check(any((g['capability'],g['api'])==(r['capability'],r['api']) for g in result),'Missing established authority '+r['capability'])
    return result
def parity(before,row,old,manifest):
    check(not row.get('image_source'),'Unqualified Wi-Fi external app layout')
    for field in ('id','file_name','entry','architecture'):
        check(old.get(field)==manifest.get(field),'Application identity differs '+field)
    for field in ('display_name','icon'):
        if field in old:check(old[field]==manifest.get(field),'Presentation differs '+field)
    shell=json.loads(before['boot.json']).get('resident_shell') or {}
    check(old['file_name'] in shell.get('foreground',[]),'Product foreground lifecycle missing; shell migration required')
def provider(before,sd,root):
    boot=json.loads(before['boot.json'])
    owners=[d for d in boot['drivers'] if any(p.get('capability')=='ui.scene' and p.get('api')==1 for p in json.loads(before[d['manifest']]).get('provides',[]))]
    check(len(owners)==1,'Scene provider missing or ambiguous')
    row=owners[0];path=safe(row['manifest']);old=json.loads(before[path])
    manifest=(root/'manifest.json').read_bytes();new=json.loads(manifest);elf=(root/'driver.elf').read_bytes()
    receipt=json.loads((root/'build.json').read_text())
    elf_receipt(receipt,elf,'driver.elf')
    check(receipt.get('manifest_sha256')==sha(manifest),'Scene manifest receipt differs')
    check(receipt.get('source_pins',{}).get('runtime')==RUNTIME and receipt.get('working_tree_dirty') is False,'Scene source receipt differs')
    source=Path(__file__).resolve().parents[1]
    check(receipt.get('source_pins',{}).get('system')==subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip(),'Scene source commit differs')
    check(receipt.get('optional_apis',{}).get('connectivity') is True,'Connectivity suffix missing')
    memory=receipt.get('memory')
    if memory is not None:
        check(type(memory.get('target_scene_model_bytes')) is int and memory['target_scene_model_bytes']>0 and type(memory.get('modal_additional_heap_bytes')) is int and memory['modal_additional_heap_bytes']>=memory['target_scene_model_bytes'],'Invalid target memory witness')
        check(memory.get('modal_max_depth')==1 and memory.get('duplicates_parent_pixel_buffers') is False and memory.get('allocator_overhead_included') is False,'Unexpected modal memory contract')
        print('SHARED WIFI SCENE MEMORY '+json.dumps({'profile_path':str(root),'measurement':memory,'product_heap_admission':False},sort_keys=True),flush=True)
    for field in ('id','file_name','driver_abi','architecture','provides','requires'):
        check(old.get(field)==new.get(field),'Scene contract changed '+field)
    licenses={}
    for name in ('LICENSE-Orbitron.txt','LICENSE-Rajdhani.txt','SOURCES.json','COMPONENT-SOURCES.json'):
        content=(root/'licenses'/name).read_bytes()
        check(content==(source/'Services/scene_host/fonts'/name).read_bytes(),'Scene license differs')
        licenses[name]=sha(content)
    after=dict(before);new_sd=dict(sd);changed={path};changed_sd=set()
    after[path]=manifest
    internal=str(PurePosixPath(path).parent/new['file_name'])
    if internal in before:
        after[internal]=elf;changed.add(internal)
    if 'image_source' in row:
        import re
        oldpath=safe(row['image_source']['path'])
        match=re.fullmatch(r'(Drivers/[^/]+)/image-([0-9a-f]{64})[.]elf',oldpath)
        check(match is not None and oldpath in sd and sha(sd[oldpath])==match.group(2),'Unknown scene SD layout')
        newpath=match.group(1)+'/image-'+sha(elf)+'.elf'
        check(newpath not in sd or sd[newpath]==elf,'Scene address collision')
        new_sd[newpath]=elf;changed_sd.add(newpath);row['image_source']['path']=newpath
        check(not row['image_source'].get('internal_fallback') or internal in before,'Scene fallback absent')
    else:check(internal in before,'Scene internal image absent')
    after['boot.json']=encode(boot);changed.add('boot.json')
    return after,new_sd,changed,changed_sd,licenses
def paths(row):
    if 'image_source' not in row:return None,None
    image=safe(row['image_source']['path'])
    return image,str(PurePosixPath(image).parent/'manifest.json')
def pack(after,stored,changed,size,build,read_image,magic,decode,encode_image):
    packed={n:stored[n] if n not in changed else encode_image(b) if n in stored and stored[n].startswith(magic) else b for n,b in after.items()}
    image,receipt=build(packed,size)
    check(len(image)==size and read_image(image,size)==packed,'SPIFFS roundtrip differs')
    check({n:decode(b) for n,b in packed.items()}==after,'Canonical envelope roundtrip differs')
    return packed,image,receipt

def native_cohort(lock,root,lane):
    """Require reviewed, hash-locked *linked product* evidence, not SDK similarity."""
    import re
    expected=lock.get('native_cohorts',{}).get(lane)
    check(isinstance(expected,dict),'Native product receipt absent: '+lane+'; host admission does not identify installed firmware')
    def digest(value):
        return isinstance(value,str) and re.fullmatch(r'[0-9a-f]{64}',value) is not None
    def artifact(row):
        check(isinstance(row,dict),'Native artifact entry absent: '+lane)
        name=safe(row.get('file'))
        path=root/name
        check(root.resolve() in path.resolve().parents and not path.is_symlink() and path.is_file(),'Native artifact absent or outside evidence root: '+name)
        b=path.read_bytes()
        check(type(row.get('size_bytes')) is int and len(b)==row['size_bytes'] and digest(row.get('sha256')) and sha(b)==row['sha256'],'Native artifact identity differs: '+name)
        return b
    receipt_bytes=artifact(expected.get('receipt'))
    r=json.loads(receipt_bytes)
    check(r.get('schema')==1 and r.get('product')==lane,'Native receipt product/schema differs')
    check(r.get('runtime_commit')==RUNTIME and r.get('runtime_commit')==expected.get('runtime_commit'),'Native Runtime differs from admission Runtime: '+lane)
    check(r.get('product_commit')==expected.get('product_commit') and isinstance(r.get('product_commit'),str) and re.fullmatch(r'[0-9a-f]{40}',r['product_commit']) is not None,'Native product source identity absent')
    check(r.get('working_tree_dirty') is False,'Native build source dirty')
    check(r.get('scope')=='linked-product-firmware' and r.get('full_firmware_linked') is True,'Native receipt is not linked product firmware')
    roles={'firmware','elf','map','profile','settings_header','compile_commands','link_log','qualification'}
    files=r.get('artifacts',{})
    check(isinstance(files,dict) and set(files)==roles and files==expected.get('artifacts'),'Native source/build evidence set differs')
    check(len({safe(row.get('file')) for row in files.values()})==len(roles),'Native artifact paths overlap')
    contents={role:artifact(files[role]) for role in sorted(roles)}
    check(contents['elf'].startswith(b'\x7fELF'),'Native ELF missing')
    q=json.loads(contents['qualification'])
    check(q.get('product')==lane and q.get('runtime_commit')==RUNTIME and q.get('product_commit')==r['product_commit'],'Native qualification source identity differs')
    check(q.get('firmware_sha256')==files['firmware']['sha256'] and q.get('elf_sha256')==files['elf']['sha256'],'Native qualification binary identity differs')
    # These are measured, reviewed product results, never inferred from a feature define.
    for result in ('capability_admission','persistent_storage_preservation','resource_budget'):
        check(q.get(result)=='passed','Native product qualification missing '+result+': '+lane)
    expected_stores={x['name']:x['sha256'] for x in lock['assets'] if x['name'].startswith('product-'+lane+'-')}
    check(r.get('baseline_assets')==expected_stores and len(expected_stores)==2,'Native receipt belongs to different companion stores')
    check(q.get('baseline_assets')==expected_stores,'Native qualification companion stores differ')
    return {'receipt_sha256':sha(receipt_bytes),'runtime_commit':r['runtime_commit'],'product_commit':r['product_commit'],'artifacts':files,'scope':'linked-product-firmware; installation and hardware operation not established'}

def arguments(description,extra=()):
    import argparse
    p=argparse.ArgumentParser(description=description)
    for n in ('system','bootfs','baseline','baseline-lock','shared','output',*extra):p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args()
    for n,v in vars(a).items():setattr(a,n,v.resolve())
    check(not a.output.exists(),'Output exists')
    return a
