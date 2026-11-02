#!/usr/bin/env python3
"""Publish qualified provider variants only after byte verification; preserve all deployment choices."""
import argparse,hashlib,json,os,subprocess,zipfile,urllib.request,urllib.error,urllib.parse
from pathlib import Path
def digest(data):return hashlib.sha256(data).hexdigest()
def run(root,runtime):
 repo=os.environ['GITHUB_REPOSITORY'];commit=os.environ['GITHUB_SHA'];token=os.environ['GITHUB_TOKEN']
 if len(commit)!=40 or any(c not in '0123456789abcdef' for c in commit):raise ValueError('Exact source SHA required')
 tag='scene-host-0.4.15-footer-'+commit[:12]
 base='https://api.github.com/repos/'+repo
 def request(url,body=None,method=None,binary=False,raw_response=False):
  headers={'Authorization':'Bearer '+token,'Accept':'application/vnd.github+json','X-GitHub-Api-Version':'2022-11-28'}
  if raw_response:headers['Accept']='application/octet-stream'
  if body is not None:headers['Content-Type']='application/octet-stream' if binary else 'application/json'
  data=body if binary else None if body is None else json.dumps(body).encode()
  with urllib.request.urlopen(urllib.request.Request(url,data=data,headers=headers,method=method)) as response:
   raw=response.read();return raw if raw_response else json.loads(raw) if raw else None
 package=root/'publication';package.mkdir(parents=True,exist_ok=True)
 evidence={}
 for lane in ('footer','footer-sanitized','footer-baseline'):
  path=Path('build')/lane/'results.json';data=path.read_bytes();obj=json.loads(data)
  assert len(obj)==5 and all(x['returncode']==(-6 if lane=='footer-baseline' else 0) for x in obj)
  evidence[lane]={'sha256':digest(data),'result':obj}
 lock={'system':commit,'runtime_sdk':subprocess.check_output(['git','-C',str(runtime),'rev-parse','HEAD'],text=True).strip()}
 (package/'qualification.json').write_text(json.dumps({'source_commits':lock,'lanes':evidence,'scope':'Actual scene provider, native navigation and touch, plus production ELF structural validation. No physical hardware qualification.'},indent=2)+'\n')
 files={'qualification.json':package/'qualification.json'}
 for profile,folder in (('full','scene'),('color-dynamic','scene-color'),('watch-compact','scene-compact')):
  basepath=root/folder/'scene-host';data=(basepath/'driver.elf').read_bytes();record=json.loads((basepath/'build.json').read_text())
  assert digest(data)==record['sha256']==record['repeat_sha256']
  name='scene-host-'+profile
  files[name+'.elf']=basepath/'driver.elf';files[name+'.json']=basepath/'manifest.json';files[name+'-build.json']=basepath/'build.json'
 for profile in ('compact-color','portrait-monochrome'):
  name='scene-profile-'+profile;basepath=root/'scene'/profile
  for other in ('scene-color','scene-compact'):
   assert (basepath/'driver.elf').read_bytes()==(root/other/profile/'driver.elf').read_bytes()
  files[name+'.elf']=basepath/'driver.elf';files[name+'.json']=basepath/'manifest.json';files[name+'-build.json']=basepath/'build.json'
 source=package/'source.zip';source.write_bytes(subprocess.check_output(['git','archive','--format=zip','HEAD']));files['source.zip']=source
 bundle=package/'scene-provider-bundle.zip'
 with zipfile.ZipFile(bundle,'w',zipfile.ZIP_DEFLATED) as z:
  for folder in ('scene','scene-color','scene-compact'):
   for path in sorted((root/folder).rglob('*')):
    if path.is_file():z.write(path,str(path.relative_to(root)))
 files[bundle.name]=bundle
 sums=package/'SHA256SUMS';sums.write_text(''.join(digest(path.read_bytes())+'  '+name+'\n' for name,path in sorted(files.items())));files['SHA256SUMS']=sums
 try:release=request(base+'/releases/tags/'+urllib.parse.quote(tag,safe=''))
 except urllib.error.HTTPError as error:
  if error.code!=404:raise
  release=request(base+'/releases',{'tag_name':tag,'target_commitish':commit,
   'name':'Scene host0.4.15 fixed-footer provider '+commit[:12],'draft':True,'prerelease':True,
   'body':'Qualified fixed-footer scene providers, full/color-dynamic/watch-compact builds and external presentation profiles. Positive, sanitized and pre-fix negative controls; deterministic target rebuilds and production ELF structural checks. Exact source and licenses included. Matching display/input/profile providers and loader memory required; physical hardware unqualified. Source '+commit})
 assert release['target_commitish']==commit and release['prerelease']
 assets={a['name']:a for a in request(base+'/releases/'+str(release['id'])+'/assets')}
 bindings=[]
 for name,path in files.items():
  data=path.read_bytes();wanted=digest(data)
  if name in assets:
   asset=assets[name]
   if asset['size']!=len(data) or asset.get('digest')!='sha256:'+wanted:raise ValueError('Existing immutable asset differs: '+name)
  else:
   asset=request(release['upload_url'].split('{')[0]+'?name='+urllib.parse.quote(name),data,binary=True)
  if asset['size']!=len(data) or asset.get('digest')!='sha256:'+wanted:raise ValueError('Uploaded asset hash mismatch: '+name)
  downloaded=request(asset['url'],raw_response=True)
  if len(downloaded)!=len(data) or digest(downloaded)!=wanted:raise ValueError('Downloaded asset hash mismatch: '+name)
  bindings.append({'name':name,'size':len(data),'sha256':wanted,'url':asset['browser_download_url']})
 if release.get('draft'):
  release=request(base+'/releases/'+str(release['id']),{'draft':False,'prerelease':True},method='PATCH')
 receipt={'repo':repo,'tag':tag,'source_commit':commit,'release_url':release['html_url'],'assets':bindings,'physical_testing':'not performed'}
 (package/'publication.json').write_text(json.dumps(receipt,indent=2)+'\n')
 print(json.dumps(receipt,indent=2))
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',type=Path,required=True);p.add_argument('--runtime',type=Path,required=True)
 a=p.parse_args();run(a.build,a.runtime)
