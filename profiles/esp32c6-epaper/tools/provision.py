#!/usr/bin/env python3
"""Build a private NVS partition; never flash hardware or contact a service."""
import argparse,csv,json,os,pathlib,subprocess,sys,tempfile

def validate_config(config):
 c=dict(config)
 for key in ['ssid','password']:
  if not isinstance(c.get(key),str):raise ValueError('Required string: '+key)
 if not 1<=len(c['ssid'].encode())<=32 or len(c['password'].encode())>63:raise ValueError('Invalid Wi-Fi fields')
 service=['site_access','participant_credential','bootstrap','enrollment_id']
 for key in service:
  c.setdefault(key,'')
  if not isinstance(c[key],str):raise ValueError('Expected string: '+key)
 if any(c[key] for key in service):
  if not 32<=len(c['participant_credential'])<=1024 or not 1<=len(c['site_access'])<=1024 or not c['bootstrap'] or not c['enrollment_id']:
   raise ValueError('Incomplete service provisioning; supply all service fields or none')
 return c

def main():
 p=argparse.ArgumentParser();p.add_argument('config');p.add_argument('output');p.add_argument('--nvs-generator',type=pathlib.Path);a=p.parse_args()
 c=validate_config(json.loads(pathlib.Path(a.config).read_text()))
 generator=a.nvs_generator
 if generator is None:
  if not os.environ.get('IDF_PATH'):p.error('Set IDF_PATH or pass --nvs-generator')
  generator=pathlib.Path(os.environ['IDF_PATH'])/'components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py'
 with tempfile.TemporaryDirectory() as d:
  csvfile=pathlib.Path(d)/'nvs.csv'
  with csvfile.open('w',newline='') as f:
   w=csv.writer(f);w.writerow(['key','type','encoding','value']);w.writerow(['epaper','namespace','','']);w.writerow(['config','data','string',json.dumps(c,separators=(',',':'))])
  subprocess.run([sys.executable,str(generator),'generate',str(csvfile),a.output,'0x20000'],check=True)
  os.chmod(a.output,0o600)
 print('Private NVS image created. Flash only to the intended board at 0x9000; this replaces its configuration and journal.')
if __name__=='__main__':main()
