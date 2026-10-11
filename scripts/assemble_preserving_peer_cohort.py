#!/usr/bin/env python3
"""Pack an offline preserving paired cohort from qualified native/store inputs.
No initial flash image, settings volume, device access or hosting is created.
"""
import argparse,hashlib,json,re,sys
from pathlib import Path

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for key in ('runtime','tools','native','store','output'):p.add_argument('--'+key,type=Path,required=True)
 a=p.parse_args();sys.path[:0]=[str(a.runtime.resolve()/'scripts'),str(a.tools.resolve()/'scripts')]
 from current_bootfs import build
 from read_only_spiffs import read_image
 from paired_candidate import native_proof,policy_rows_proof,partitions,APP_DATA_EXPECTED
 from radio_iq_proof import prove
 from release_assets import esp_image
 sha=lambda b:hashlib.sha256(b).hexdigest()
 blobs={n:(a.native/n).read_bytes() for n in ('firmware.bin','firmware.elf','partitions.bin','bootloader.bin')}
 esp_image(blobs['firmware.bin']);partitions(blobs['partitions.bin'],APP_DATA_EXPECTED)
 proof={'native':native_proof(blobs['firmware.elf']),'policy':policy_rows_proof(blobs,24,24),'iq':prove(blobs['firmware.elf'])}
 store={str(f.relative_to(a.store)):f.read_bytes() for f in a.store.rglob('*') if f.is_file()}
 cohort=json.loads(store['cohort.json']);cohort.update(firmware_size=len(blobs['firmware.bin']),firmware_sha256=sha(blobs['firmware.bin']))
 assert ('RISC_RUNTIME_VERSION:'+cohort['runtime_version']).encode()+b'\0' in blobs['firmware.bin']
 store['cohort.json']=(json.dumps(cohort,sort_keys=True,indent=2)+'\n').encode()
 image,packing=build(store);assert len(image)==0x510000 and read_image(image,0x510000)==store
 payload=blobs['firmware.bin']+image
 assert len(payload)==cohort['firmware_size']+0x510000
 a.output.mkdir(parents=True,exist_ok=False)
 for name,data in store.items():
  path=a.output/'store'/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
 name=cohort['product']+'-cohort-'+cohort['version']+'.bin';(a.output/name).write_bytes(payload)
 (a.output/'bootfs.bin').write_bytes(image)
 metadata={k:v for k,v in cohort.items() if k not in ('schema','schema_version')}
 metadata.update(kind='paired-cohort',asset=name,size=len(payload),sha256=sha(payload),store_size=len(image),store_sha256=sha(image))
 (a.output/'candidate.json').write_text(json.dumps(metadata,indent=2)+'\n')
 (a.output/'qualification.json').write_text(json.dumps({'native_proof':proof,'packing':packing,'store_sha256':{n:sha(b) for n,b in store.items()},'payload_contains':['native firmware','complete boot store'],'appdata_included':False,'nvs_included':False,'hardware_tested':False},indent=2)+'\n')
 print(name,len(payload),sha(payload))
if __name__=='__main__':main()
