#!/usr/bin/env python3
"""Build a private NVS partition from config JSON using the pinned ESP-IDF SDK.
This creates an image; it does not flash a connected device.
"""
import argparse,csv,json,os,pathlib,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('config');p.add_argument('output');a=p.parse_args()
c=json.loads(pathlib.Path(a.config).read_text())
for k in ['ssid','password','site_access','participant_credential','bootstrap','enrollment_id']:
 if not isinstance(c.get(k),str):raise ValueError('Required string: '+k)
if not 1<=len(c['ssid'].encode())<=32 or len(c['password'].encode())>63:raise ValueError('Invalid Wi-Fi fields')
if not 32<=len(c['participant_credential'])<=1024 or not 1<=len(c['site_access'])<=1024:raise ValueError('Invalid credential lengths')
with tempfile.TemporaryDirectory() as d:
 csvfile=pathlib.Path(d)/'nvs.csv'
 with csvfile.open('w',newline='') as f:
  w=csv.writer(f);w.writerow(['key','type','encoding','value']);w.writerow(['epaper','namespace','','']);w.writerow(['config','data','string',json.dumps(c,separators=(',',':'))])
 generator=pathlib.Path(os.environ['IDF_PATH'])/'components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py'
 subprocess.run(['python',str(generator),'generate',str(csvfile),a.output,'0x20000'],check=True)
 os.chmod(a.output,0o600)
print('Private NVS image created. Flash only to the intended board at 0x9000; this replaces its device configuration and journal.')
