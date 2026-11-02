#!/usr/bin/env python3
"""Retain actual completed owning CI receipts; queued/failed runs never qualify."""
import argparse,json,os,re,urllib.request
from pathlib import Path
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--lock',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
 a=p.parse_args();lock=json.loads(a.lock.read_text())
 if a.output.exists() or not lock:raise ValueError('Fresh nonempty owning CI receipt required')
 headers={'Accept':'application/vnd.github+json','X-GitHub-Api-Version':'2022-11-28'}
 if os.environ.get('GH_TOKEN'):headers['Authorization']='Bearer '+os.environ['GH_TOKEN']
 def read(url):
  with urllib.request.urlopen(urllib.request.Request(url,headers=headers),timeout=120) as response:return json.load(response)
 receipts=[]
 for item in lock:
  if not re.fullmatch(r'michaelrolphone-cmyk/[A-Za-z0-9_.-]+',item['repository']) or not re.fullmatch('[0-9a-f]{40}',item['head_sha']):raise ValueError('Exact owning source required')
  base='https://api.github.com/repos/'+item['repository']+'/actions/runs/'+str(int(item['run_id']))
  run=read(base)
  if run['head_sha']!=item['head_sha'] or run['name']!=item['workflow'] or run['status']!='completed' or run['conclusion']!='success':raise ValueError('Owning CI not successful at exact source '+base)
  jobs=[];page=1
  while True:
   batch=read(base+'/jobs?filter=latest&per_page=100&page='+str(page));jobs+=batch['jobs']
   if len(jobs)>=batch['total_count']:break
   page+=1
  if not jobs or any(j['status']!='completed' or j['conclusion']!='success' for j in jobs):raise ValueError('Owning CI job incomplete or failed '+base)
  receipts.append({'expected':item,'run':run,'jobs':jobs})
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps({'schema':1,'scope':'Actual hosted workflow/job completion at exact owning commits; no physical testing claim','receipts':receipts},sort_keys=True,indent=2)+'\n')
 print('Exact owning hosted CI receipts verified:',len(receipts))
if __name__=='__main__':main()
