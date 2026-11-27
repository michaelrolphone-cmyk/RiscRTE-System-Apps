"""Bind historical scene preservation separately from current behavioral tests."""
import hashlib,subprocess
from pathlib import Path
SCENE='55c94bde412dec7772bedac29b067bcd83aad802'
BEFORE='be142dbec17881ac09e4b1617606d161a8bf5204'
def bind(root,revision):
 root=Path(root).resolve()
 def git(*args):return subprocess.check_output(['git','-C',str(root),*args])
 if git('rev-parse','HEAD').decode().strip()!=revision:raise ValueError('Wrong scene source revision')
 paths=['lib/PortableApps','lib/NativeApps','Apps','test/native_apps']
 if git('status','--porcelain','--untracked-files=all','--',*paths):raise ValueError('Dirty scene source role')
 sources={}
 for row in git('ls-tree','-rz',revision,'--',*paths).split(b'\0'):
  if not row:continue
  meta,name=row.split(b'\t');mode,kind,oid=meta.split();p=root/name.decode()
  # Historical sparse worktrees may omit test fixtures, which parity never consumes.
  if name.startswith(b'test/') and not p.exists():continue
  if kind!=b'blob' or mode not in (b'100644',b'100755') or p.is_symlink():raise ValueError('Unbound scene input')
  raw=p.read_bytes()
  if hashlib.sha1(b'blob '+str(len(raw)).encode()+b'\0'+raw).hexdigest()!=oid.decode():raise ValueError('Changed scene input: '+str(p))
  sources[name.decode()]=hashlib.sha256(raw).hexdigest()
 return {'commit':revision,'root':str(root),'sources':sources}
def historical(scene,before):
 return {'scene':bind(scene,SCENE),'before_scene':bind(before,BEFORE)}
