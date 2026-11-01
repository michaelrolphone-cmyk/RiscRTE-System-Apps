#!/usr/bin/env python3
"""Repackage verified v1.6.3 native bytes with the freshly built role ELF cohort."""
import argparse,hashlib,json,pathlib,shutil,subprocess,sys,zipfile
BASE_SHA='666aa1c1a43d3547d2fbe623a12a3f573bc401470fc49beaa5351fcc1b5e12b6'
BASE_URL='https://github.com/michaelrolphone-cmyk/RiscRTE/releases/tag/esp32s3-infrastructure-v1.6.3'
ROOT=pathlib.Path(__file__).resolve().parents[1]
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser();p.add_argument('--base-archive',required=True,type=pathlib.Path);p.add_argument('--runtime',required=True,type=pathlib.Path);p.add_argument('--elf',required=True,type=pathlib.Path);p.add_argument('--idf',required=True,type=pathlib.Path);p.add_argument('--output',required=True,type=pathlib.Path);a=p.parse_args()
 if digest(a.base_archive)!=BASE_SHA:raise ValueError('Published base digest mismatch')
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);stage=out/'work';stage.mkdir();base=stage/'base';base.mkdir()
 with zipfile.ZipFile(a.base_archive) as z:
  for name in z.namelist():
   if pathlib.PurePosixPath(name).is_absolute() or '..' in pathlib.PurePosixPath(name).parts:raise ValueError('Unsafe archive path')
  z.extractall(base)
 manifests=list(base.glob('*/flash-manifest.json'))
 if len(manifests)!=1:raise ValueError('Expected one complete base bundle')
 base=manifests[0].parent
 # The existing seed builder verifies each vendored LittleFS file by hash.
 tools=ROOT/'tools/littlefs';tools.parent.mkdir(exist_ok=True)
 if not tools.exists():tools.symlink_to((base/'tools/littlefs').resolve(),target_is_directory=True)
 evidence=stage/'evidence';evidence.mkdir();shutil.copy2(a.elf/'elf-build.json',evidence/'elf-build.json')
 (evidence/'source.json').write_text(json.dumps({'system_apps':subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip(),'physical_qualification':'not_tested'}))
 bundle=stage/'esp32s3-infrastructure-v1.8.0'
 subprocess.run([sys.executable,str(ROOT/'scripts/repackage_infrastructure.py'),'--base-bundle',str(base),'--base-archive',str(a.base_archive.resolve()),'--base-archive-sha256',BASE_SHA,'--base-release-url',BASE_URL,'--runtime',str(a.runtime.resolve()),'--elf',str(a.elf.resolve()),'--spiffsgen',str(a.idf/'components/spiffs/spiffsgen.py'),'--output',str(bundle),'--evidence',str(evidence)],check=True)
 manifest=json.loads((bundle/'flash-manifest.json').read_text());image=bytearray(b'\xff'*manifest['flash_bytes']);ranges=[]
 for item in manifest['files']:
  if 'offset' not in item:continue
  data=(bundle/item['path']).read_bytes();start=item['offset'];end=start+len(data)
  if end>len(image) or any(start<b and a<end for a,b in ranges):raise ValueError('Overlapping or out-of-bounds image')
  if digest(bundle/item['path'])!=item['sha256']:raise ValueError('Component mismatch')
  image[start:end]=data;ranges.append((start,end))
 release=out/'release';release.mkdir();binary=release/'riscrte-esp32s3-infrastructure-v1.8.0-initial.bin';binary.write_bytes(image)
 shutil.copy2(bundle/'flash-manifest.json',release/'flash-manifest.json');shutil.copy2(str(bundle)+'.zip',release/'riscrte-esp32s3-infrastructure-v1.8.0-repair.zip')
 (release/'direct-image.json').write_text(json.dumps({'asset':binary.name,'bytes':len(image),'sha256':digest(binary),'source':manifest['source'],'first_install_only':True},indent=2)+'\n')
 (release/'RELEASE-NOTES.md').write_text('# ESP32-S3 infrastructure v1.8.0\n\nAutomatic attachment-time role allocation across Explorer, Sentinel, Reconciler and Continuity. Renewable assignments and durable drain-before-switch transitions; service and feature demands select eligible local profiles. Uses unchanged verified v1.6.3 native runtime with freshly compiled product ELFs. See AGENT-MODES.md inside the repair bundle for role selection, distinct device credentials, Sentinel challenge subscriptions and shared relay setup.\n\nThe initial image is FIRST INSTALL ONLY and replaces device data. Existing devices must use the documented preserving repair/update procedure. No device has been flashed or physically qualified by this build. Shared relay delivery requires receiver idempotency and individually provisioned static callback credentials; sealed dynamic enrollment remains single-executor.\n')
 subprocess.run([sys.executable,str(a.runtime/'scripts/verify_infrastructure_initial_image.py'),str(release)],check=True)
 (release/'SHA256SUMS').write_text(''.join(digest(f)+'  '+f.name+'\n' for f in sorted(release.iterdir()) if f.is_file()))
 print(release)
if __name__=='__main__':main()
