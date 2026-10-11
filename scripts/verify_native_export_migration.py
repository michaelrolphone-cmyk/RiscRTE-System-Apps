#!/usr/bin/env python3
"""Read the actual linked V2 record and compare it with both selected stores."""
import argparse,io,json,struct
from pathlib import Path
from elftools.elf.elffile import ELFFile
p=argparse.ArgumentParser(description=__doc__)
for n in ('elf','source','target','output'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();e=ELFFile(io.BytesIO(a.elf.read_bytes()));symbols=list(e.get_section_by_name('.symtab').iter_symbols())
records=[s for s in symbols if 'risc_native_app_data_export_policy_v2_record' in s.name and s['st_info']['type']=='STT_OBJECT'];assert len(records)==1 and records[0]['st_size']==64

sections=[(section['sh_addr'],section.data()) for section in e.iter_sections() if section['sh_type']!='SHT_NOBITS' and section['sh_flags']&2]
def raw(address,size):
 for start,data in sections:
  offset=address-start
  if 0<=offset and offset+size<=len(data):return data[offset:offset+size]
 raise AssertionError('Pointer outside loaded ELF sections')
def string(address):
 assert address
 out=bytearray()
 for i in range(65536):
  b=raw(address+i,1)
  if b==b'\0':return out.decode()
  out+=b
 raise AssertionError('Unbounded native string')
values=struct.unpack('<16I',raw(records[0]['st_value'],64));assert values[0]==2
for start,root in [(2,a.source),(7,a.target)]:
 c=json.loads((root/'cohort.json').read_text())
 for i,k in enumerate(('product','version','source_repo','source_revision','runtime_version')):assert string(values[start+i])==c[k],k
for index,root in [(12,a.source),(13,a.target)]:assert json.loads(string(values[index]))==json.loads((root/'boot.json').read_text())['app_capabilities']
kv=[]
for i in range(values[15]):
 consumer,api,ns,owners,count=struct.unpack('<5I',raw(values[14]+20*i,20));assert count<=64
 kv.append({'consumer':string(consumer),'api':api,'namespace':ns,'previous_owners':[string(struct.unpack('<I',raw(owners+4*j,4))[0]) for j in range(count)]})
record={'schema':2,'migration_id':string(values[1]),'linked_record_bytes':64,'source_and_target_snapshots_verified':True,'shared_kv':kv,'selected_function_present':any(s.name=='risc_native_app_data_export_policy_v2' and s['st_value'] for s in symbols)}
assert record['selected_function_present'];a.output.write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record))
