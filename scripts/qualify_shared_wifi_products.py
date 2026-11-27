#!/usr/bin/env python3
"""Offline product preservation, pack roundtrip and selected Runtime admission."""
import copy,json,os,subprocess,sys
from pathlib import Path,PurePosixPath
from shared_wifi_products import *
ROOT=Path(__file__).resolve().parents[1]
def command(cmd,log):
    q=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1'})
    log.write_text(q.stdout+q.stderr);check(q.returncode==0,'Command failed: '+str(log));return q.stdout
def preservation(before,before_sd,after,sd,elf,manifest,provider_root):
    from assemble_shared_wifi_products import assemble
    expected,expected_sd,changed,changed_sd,licenses=assemble(before,before_sd,elf,manifest,provider_root)
    check(after==expected,'Candidate internal state differs from constrained assembly')
    check(sd==expected_sd,'Candidate SD state differs from constrained assembly')
    for n,b in before.items():
        check(n in changed or after.get(n)==b,'Internal peer changed '+n)
    for n,b in before_sd.items():
        check(n in changed_sd or sd.get(n)==b,'SD peer changed '+n)
    check(set(before)<=set(after) and set(before_sd)<=set(sd),'Published peer deleted')
    return changed,licenses
def main():
    a=arguments(__doc__,('runtime','candidate','providers'));pin(a.runtime,RUNTIME)
    elf,manifest,lanes,build,read_image,magic,decode,encode_image=load(a)
    a.output.mkdir(parents=True)
    receipt={'schema':1,'status':'running','runtime':RUNTIME,'system':SYSTEM,'bootfs':BOOTFS,'elf_sha256':sha(elf),'manifest_sha256':sha(manifest),'products':{},'physical_testing':False,'target_execution':False,'installed_predecessor_qualified':False}
    metadata={}
    for lane,(size,stored,before,before_sd) in lanes.items():
        after=tree(a.candidate/lane/'after');sd=tree(a.candidate/lane/'after-sd')
        changed,licenses=preservation(before,before_sd,after,sd,elf,manifest,a.providers/lane/'scene-host')
        packed,image,packing=pack(after,stored,changed,size,build,read_image,magic,decode,encode_image)
        out=a.output/lane;out.mkdir();write(out/'before',before);write(out/'stored',packed)
        (out/'store.bin').write_bytes(image);zip_roundtrip(out/'sd-apps.zip',sd)
        receipt['products'][lane]={'licenses':licenses,'before_selection':selected(before)[2],'after_selection':selected(after)[2],'packing':packing,'canonical_files':{n:sha(b) for n,b in after.items()},'stored_files':{n:sha(b) for n,b in packed.items()},'sd_files':{n:sha(b) for n,b in sd.items()},'encoded_images':sum(b.startswith(magic) for b in packed.values())}
        image_path,_=paths(selected(after)[2])
        stored_image=sd[image_path] if image_path else packed['wifi_settings.elf']
        receipt['products'][lane]['image_storage']={'location':'sd' if image_path else 'internal','canonical_bytes':len(elf),'stored_bytes':len(stored_image)}
        print('SHARED WIFI PACK '+json.dumps({'product':lane,'packing':packing,'image_storage':receipt['products'][lane]['image_storage']},sort_keys=True),flush=True)
    (a.output/'roundtrip.json').write_bytes(encode(receipt))
    fixtures=a.system/'test/files_products/native';templates=json.loads((fixtures/'commands.json').read_text())
    for mode in ('normal','sanitized'):
        out=a.output/mode;out.mkdir()
        san=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer',*(['-no-pie'] if sys.platform=='linux' else [])] if mode=='sanitized' else []
        inc=['-I'+str(a.runtime/x) for x in ('src','sdk/app','sdk/driver','sdk/hardware','lib/ArduinoJson/src','lib/elf_loader/include','test/native_bank_stubs')]
        validator=out/'validator.o'
        command(['cc','-std=c11',*san,*inc,'-c',a.runtime/'lib/elf_loader/src/esp_elf_validate.c','-o',validator],out/'validator.log')
        command(['c++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-unused-function',*san,*inc,'-I'+str(fixtures),ROOT/'test/shared_wifi/elf_admission.cpp',validator,'-o',out/'elf-admission'],out/'elf-compile.log')
        command([out/'elf-admission',a.shared/'wifi_settings.elf'],out/'elf-admission.log')
        for lane,template in templates.items():
            cmd=[x.format(runtime=a.runtime,x4_runtime=a.runtime,fixtures=fixtures,output=out) for x in template]
            cmd[1:1]=[*san,'-DSTORE_ADMISSION_COHORT','-Wno-misleading-indentation',*(['-DSTORE_ADMISSION_ENCODED_IMAGES'] if lane!='x4' else [])]
            cmd.extend([*inc,'-I'+str(ROOT/'test/shared_wifi'),str(validator)])
            (out/(lane+'-command.json')).write_bytes(encode(cmd));command(cmd,out/(lane+'-compile.log'))
            argv=[out/lane,a.output/lane/'before',a.output/lane/'stored']
            r=json.loads(command(argv,out/(lane+'-admission.log')))
            check(r['prepared'] and r['cohort_validated'] and r['elf_count'] and not r['hardware_calls'] and not r['storage_calls'],'Runtime admission failed '+lane)
            check(r.get('encoded_image_count',0)==receipt['products'][lane]['encoded_images'],'Envelope count mismatch')
            if lane=='x4':check(r.get('retained_sd_serial_graph_bound'),'X4 SD graph admission missing')
            receipt['products'][lane][mode]=r
            print('SHARED WIFI ADMISSION '+json.dumps({'product':lane,'mode':mode,'result':r},sort_keys=True),flush=True)
            (a.output/'qualification.json').write_bytes(encode(receipt))
    pin(a.runtime,RUNTIME);pin(a.system,SYSTEM);pin(a.bootfs,BOOTFS)
    # Keep host graph/ELF evidence separate from the real native firmware cohort.
    lock=json.loads(a.baseline_lock.read_text())
    receipt['status']='native_identity_pending'
    (a.output/'qualification.json').write_bytes(encode(receipt))
    for lane in LANES:
        receipt['products'][lane]['native_cohort']=native_cohort(lock,a.baseline,lane)
        (a.output/'qualification.json').write_bytes(encode(receipt))
    receipt['native_cohort_qualified']=True
    receipt['status']='passed';(a.output/'qualification.json').write_bytes(encode(receipt))
    print('PASS: shared Wi-Fi product preservation, roundtrips, host Runtime admission and hash-locked native cohort (not installed/hardware qualification)')
if __name__=='__main__':main()
