"""Package identity gate using System's owning release-parity version ordering."""
import hashlib,json
from check_release_parity import newer_version

def check_identity(name,old_manifest,new_manifest,old_elf,new_elf):
 old=json.loads(old_manifest);new=json.loads(new_manifest)
 changed=old_elf!=new_elf or old!=new
 if changed:
  if not newer_version(new['version'],old['version']):
   raise ValueError(name+': changed package requires a newer semantic version')
 else:
  assert old['version']==new['version']
 return {'package':name,'before_version':old['version'],'after_version':new['version'],
         'before_elf_sha256':hashlib.sha256(old_elf).hexdigest(),
         'after_elf_sha256':hashlib.sha256(new_elf).hexdigest(),
         'changed':changed,'owning_version_guard':'check_release_parity.newer_version','passed':True}
