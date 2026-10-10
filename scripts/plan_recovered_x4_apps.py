#!/usr/bin/env python3
"""Generate source-only commands for eight System and nine Utilities X4 apps.
No target execution, installed ELF or BIN input; product composition is separate.
"""
import argparse,hashlib,json,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def git(p,*a):return subprocess.check_output(['git','-C',str(p),*a],text=True).strip()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('utilities','runtime','product','display-sdk','msc-sdk','compiler','catalog','output'):
  p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();s=ROOT;u=a.utilities.resolve();r=a.runtime.resolve();product=a.product.resolve();sdk=a.display_sdk.resolve();msc=a.msc_sdk.resolve();out=a.output.resolve();catalog=a.catalog.resolve()
 if any(out==q or out.is_relative_to(q) for q in (s,u,r,product)):raise ValueError('Use output outside every source tree')
 sleep=product/'minimal/apps/portable_sleep.c';idle=product/'minimal/apps/portable_idle_sleep.c'
 assert sha(sleep)=='228f4f58bd9a3eb9c93087305fd8a6facef769cc285ec5904f4c6e78cd2babd2'
 assert sha(idle)=='8bef69d648dc1c8aad12adb24c90a2a48320287a74ff969ca33d9186fc3ab297'
 required=('RiscDisplayOutputV1.h','RiscDisplayOutputPowerV1.h','RiscDisplayOutputMetricsV1.h','RiscDisplayOutputSnapshotV1.h','RiscDisplayOutputFrontlightV1.h','RiscDisplayOutputSettledV1.h','RiscTouchV1.h','RiscTouchPowerV1.h','RiscStorageVolumeV1.h')
 for name in required:assert (sdk/name).is_file(),name
 assert (msc/'RiscUsbDeviceMscV1.h').is_file()
 c=['--resident-shell-client','--resident-runtime-sdk',str(r/'sdk/app'),'--resident-policy','--alarm-client','--home-app','default.elf','--tagged-alarm-utilities',str(u),'--native-time-runtime-repo',str(r),'--touch-scrolling','--ble-broadcast']
 cmd={}
 cmd['default']=[sys.executable,str(s/'scripts/build_paper_clock.py'),'--desk-clock','--sparse-start','--desk-lock-home','--desk-points-face','--navigation','--alarm-client','--wake-light-restore','--local-sleep-source',str(sleep),'--sleep-capability','x4.power','--sleep-sdk',str(sdk),'--retained-wake-sdk',str(r/'sdk/app'),'--tagged-alarm-utilities',str(u),'--paper-crossfade','--paper-display-sdk',str(sdk),'--paper-transitions','--quick-actions','--quick-radios','--quick-usb-transfer','--resident-shell-host','--resident-runtime-sdk',str(r/'sdk/app'),'--ble-broadcast','--x4-idle-source',str(idle),'--x4-idle-sdk',str(sdk),'--x4-idle-runtime-sdk',str(r/'sdk/driver'),'--contexts-rf-only','--resident-legacy-handoff','--resident-policy','--resident-loading-catalog',str(catalog),'--frontlight-tone','--frontlight-tone-sdk',str(sdk),'--display-settled-sdk',str(sdk),'--crash-report-sd','--crash-report-spool-namespace','62']
 cmd['springboard']=[sys.executable,str(s/'scripts/build_portable_springboard.py'),*c,'--time-profile','x4-native-time','--catalog',str(catalog),'--return-app','default.elf','--paper-crossfade','--paper-display-sdk',str(sdk)]
 cmd['settings']=[sys.executable,str(s/'scripts/build_portable_settings.py'),*c,'--settings-profile','x4-native-time','--settings-list-scrolling','--home-desk-lock','--unpadded-hours','--return-app','springboard.elf']
 selected=[*c,'--navigation','--time-profile','x4-native-time','--stage-logs']
 cmd['file_browser']=[sys.executable,str(s/'scripts/build_portable_file_browser.py'),*selected,'--storage-capability','storage.volume','--storage-instance','9','--file-handlers','--return-app','springboard.elf']
 cmd['wifi_settings']=[sys.executable,str(s/'scripts/build_portable_wifi.py'),*selected,'--wifi-instance','15','--return-app','springboard.elf']
 cmd['updates']=[sys.executable,str(s/'scripts/build_portable_updates.py'),*selected,'--product','x4','--wifi-instance','15','--apps-only']
 cmd['usb_sd_transfer']=[sys.executable,str(s/'scripts/build_portable_usb_transfer.py'),'--resident-shell-client','--resident-runtime-sdk',str(r/'sdk/app'),'--resident-policy','--tagged-alarm-utilities',str(u),'--msc-sdk',str(msc)]
 for name,command in cmd.items():command+=['--output-dir',str(out/name)]
 cmd['utilities']=[sys.executable,str(u/'scripts/build_x4_resident_clients.py'),'--resident-shell-client','--system-apps',str(s),'--system-revision',git(s,'rev-parse','HEAD'),'--runtime',str(r),'--runtime-revision',git(r,'rev-parse','HEAD'),'--display-sdk',str(sdk),'--output',str(out/'utilities')]
 record={'schema':1,'scope':'8 System + 9 Utilities applications, source-only clean build plan','sources':{name:{'path':str(repo),'commit':git(repo,'rev-parse','HEAD'),'tree':git(repo,'rev-parse','HEAD^{tree}'),'tracked_dirty':bool(git(repo,'status','--porcelain','--untracked-files=no'))} for name,repo in [('system',s),('utilities',u),('runtime',r)]},'environment':{'NATIVE_APP_CC':str(a.compiler.resolve()),'PLATFORMIO_SETTING_ENABLE_TELEMETRY':'No'},'commands':cmd,'external_sources':{str(sleep):sha(sleep),str(idle):sha(idle)},'display_sdk_sha256':{p.name:sha(p) for p in sdk.glob('*.h')},'catalog_sha256':sha(catalog),'other_app_recipes_required':['Points','Timecard','Contexts','GameBoy'],'existing_binary_inputs':[],'qualification':'Plan only. Execute only after source freeze, in a new empty output; full target admission/store graph/firmware checks remain mandatory.','public_alarm_sdk_objects_required':['637e13b0bce62ad49b756bec2468a6271d163fc7','e80172353fcb9e6519ca9d9b2a4d43c69c1bc91d']}
 out.mkdir(parents=True,exist_ok=True);(out/'commands.json').write_text(json.dumps(record,indent=2)+'\n');print(out/'commands.json')
if __name__=='__main__':main()
