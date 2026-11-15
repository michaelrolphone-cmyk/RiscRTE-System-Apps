#!/usr/bin/env python3
"""Preserve the published Home closure while replacing only shared Touchpad and ui.scene."""
import argparse,copy,hashlib,json,sys,urllib.request,zipfile,io
from pathlib import Path,PurePosixPath
from compose_shared_app_cohort import safe,encode,digest,write_tree
from image_envelope import MAGIC,decode_image,encode_image,receipt as envelope_receipt
from assemble_shared_home_cohort import inventory,fingerprint,pack_candidate
BASELINE={
  "assembly.json": {
    "url": "https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/releases/download/home-shared-83afd19e5184eb511cc205299c83eb9cb5c05430/assembly.json",
    "sha256": "9c0b333a20bbe95a13e13bcfcfd5ec4aba7fe575a61598f7f36269bc568bacb5",
    "size": 43444
  },
  "home-delivery.json": {
    "url": "https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/releases/download/home-shared-83afd19e5184eb511cc205299c83eb9cb5c05430/home-delivery.json",
    "sha256": "1a8eb4c56c3cbc5faa8edcc38d9592c25b4d7ccb5a77680483544ca73c630dde",
    "size": 37793
  },
  "product-watch-sd-sd-apps.zip": {
    "url": "https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/releases/download/home-shared-83afd19e5184eb511cc205299c83eb9cb5c05430/product-watch-sd-sd-apps.zip",
    "sha256": "8739c76e681f900923b900c9df0ef75cf421d39cabb54650c4b9ad19b6a76d85",
    "size": 22
  },
  "product-watch-store-store.bin": {
    "url": "https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/releases/download/home-shared-83afd19e5184eb511cc205299c83eb9cb5c05430/product-watch-store-store.bin",
    "sha256": "01f19575dd163c9ffe9a29fc660794895ae227b9a5413afb6dc2818695c1e6dd",
    "size": 5570560
  },
  "product-wave-sd-sd-apps.zip": {
    "url": "https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/releases/download/home-shared-83afd19e5184eb511cc205299c83eb9cb5c05430/product-wave-sd-sd-apps.zip",
    "sha256": "9008ae2c1f7cdfb21c301667d0669cd994c287990a638fbd824de1421c349b22",
    "size": 1767408
  },
  "product-wave-store-store.bin": {
    "url": "https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/releases/download/home-shared-83afd19e5184eb511cc205299c83eb9cb5c05430/product-wave-store-store.bin",
    "sha256": "84d83fc6386ead2ccbf3059ea87cd3d23ec80a918d6aef891a4449a449eae293",
    "size": 5570560
  },
  "product-x4-sd-sd-apps.zip": {
    "url": "https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/releases/download/home-shared-83afd19e5184eb511cc205299c83eb9cb5c05430/product-x4-sd-sd-apps.zip",
    "sha256": "c1948739143957840a57ed3d9de4b5f7359d42be93df7b670386c7c56498f59a",
    "size": 5444854
  },
  "product-x4-store-store.bin": {
    "url": "https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/releases/download/home-shared-83afd19e5184eb511cc205299c83eb9cb5c05430/product-x4-store-store.bin",
    "sha256": "85a20e1d31b9cbd8a668b2a0032de7644f3796d868ea966fe2a55846aa285bfe",
    "size": 5308416
  }
}
BASELINE.update({
  "native-watch-descriptor.json": {
    "url": "https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/releases/download/home-shared-83afd19e5184eb511cc205299c83eb9cb5c05430/native-watch-descriptor.json",
    "sha256": "c81ecf336d43603864decd8dbe4f92b78c9bb8a5802919f040f6e499c766a574",
    "size": 692
  },
  "native-watch-firmware.bin": {
    "url": "https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/releases/download/home-shared-83afd19e5184eb511cc205299c83eb9cb5c05430/native-watch-firmware.bin",
    "sha256": "d55b65af993adb40018ed5d8f990f8cedb26382ba612e5bd7f13c74395d66313",
    "size": 1634624
  },
  "native-watch-qualification.json": {
    "url": "https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/releases/download/home-shared-83afd19e5184eb511cc205299c83eb9cb5c05430/native-watch-qualification.json",
    "sha256": "509a8373d91507c219b88eae4d057957a0fd4e92d2eff3d19d618488e1023876",
    "size": 203741
  },
  "native-wave-descriptor.json": {
    "url": "https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/releases/download/home-shared-83afd19e5184eb511cc205299c83eb9cb5c05430/native-wave-descriptor.json",
    "sha256": "ffd8637e37bf361402efe8e8aea9690226d030490cd033cab6c8199a468d31db",
    "size": 711
  },
  "native-wave-firmware.bin": {
    "url": "https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/releases/download/home-shared-83afd19e5184eb511cc205299c83eb9cb5c05430/native-wave-firmware.bin",
    "sha256": "cb3985c2e40e93d0b9c49e82055bd578fdf716677aedf9b0afef52b6df4f2f66",
    "size": 1819248
  },
  "native-wave-qualification.json": {
    "url": "https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/releases/download/home-shared-83afd19e5184eb511cc205299c83eb9cb5c05430/native-wave-qualification.json",
    "sha256": "67bbc10301e8b4d2ada9fa59cf4339c236903e109d5fef5eb7d4ccb0eabef70c",
    "size": 208320
  },
  "native-x4-descriptor.json": {
    "url": "https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/releases/download/home-shared-83afd19e5184eb511cc205299c83eb9cb5c05430/native-x4-descriptor.json",
    "sha256": "3746698738056a2ca1d4a15c79c0ee20ff80936331e5cbbe80f5067373f23911",
    "size": 694
  },
  "native-x4-firmware.bin": {
    "url": "https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/releases/download/home-shared-83afd19e5184eb511cc205299c83eb9cb5c05430/native-x4-firmware.bin",
    "sha256": "ae5206ac566f9651d693c9834b17a39173ff961f2f9e580e828931125cf6f101",
    "size": 1816128
  },
  "native-x4-qualification.json": {
    "url": "https://github.com/michaelrolphone-cmyk/RiscRTE-System-Apps/releases/download/home-shared-83afd19e5184eb511cc205299c83eb9cb5c05430/native-x4-qualification.json",
    "sha256": "628097ddff6866fce3f7e48123e1337990030e6368801a0e303c1d067eb536f8",
    "size": 12495
  }
})
AUTHORITY={
  "watch": {
    "app_row": {
      "grants": [
        {
          "api": 1,
          "capability": "display.output",
          "instance_id": 5
        },
        {
          "api": 1,
          "capability": "input.touch.raw",
          "instance_id": 6
        },
        {
          "api": 1,
          "capability": "board.battery",
          "instance_id": 4
        },
        {
          "api": 1,
          "capability": "bluetooth.hid",
          "instance_id": 0
        },
        {
          "api": 1,
          "capability": "alarm.service",
          "instance_id": 0
        },
        {
          "api": 1,
          "capability": "storage.key-value",
          "instance_id": 1
        },
        {
          "api": 2,
          "capability": "rtc.clock",
          "instance_id": 8
        },
        {
          "api": 1,
          "capability": "net.wifi",
          "instance_id": 15
        },
        {
          "api": 1,
          "capability": "bluetooth.hci",
          "instance_id": 16
        },
        {
          "api": 1,
          "capability": "motion.accel",
          "instance_id": 7
        },
        {
          "api": 1,
          "capability": "telemetry.broadcast",
          "instance_id": 0
        },
        {
          "api": 1,
          "capability": "contexts.service",
          "instance_id": 0
        },
        {
          "api": 1,
          "capability": "input.power-key",
          "instance_id": 0
        },
        {
          "api": 1,
          "capability": "power.sleep",
          "instance_id": 0
        },
        {
          "api": 1,
          "capability": "display.power",
          "instance_id": 0
        }
      ],
      "manifest": "ble_touchpad.json"
    },
    "app_manifest": {
      "architecture": "xtensa-esp32s3",
      "display_name": "BLE Touchpad",
      "entry": "app_main",
      "file_name": "ble_touchpad.elf",
      "icon": "solid:f245",
      "id": "ble_touchpad",
      "requires": [
        {
          "api": 1,
          "capability": "display.output"
        },
        {
          "api": 1,
          "capability": "input.touch.raw"
        },
        {
          "api": 2,
          "capability": "rtc.clock"
        },
        {
          "api": 1,
          "capability": "alarm.service"
        },
        {
          "api": 1,
          "capability": "telemetry.broadcast"
        },
        {
          "api": 1,
          "capability": "contexts.service"
        },
        {
          "api": 1,
          "capability": "input.power-key"
        },
        {
          "api": 1,
          "capability": "power.sleep"
        },
        {
          "api": 1,
          "capability": "display.power"
        },
        {
          "api": 1,
          "capability": "board.battery"
        },
        {
          "api": 1,
          "capability": "storage.key-value"
        },
        {
          "api": 1,
          "capability": "net.wifi"
        },
        {
          "api": 1,
          "capability": "bluetooth.hci"
        },
        {
          "api": 1,
          "capability": "motion.accel"
        },
        {
          "api": 1,
          "capability": "bluetooth.hid"
        }
      ],
      "type": "application",
      "version": "0.1.17"
    },
    "scene_row": {
      "manifest": "ui-scene/manifest.json"
    },
    "scene_manifest": {
      "architecture": "xtensa-esp32s3",
      "driver_abi": 2,
      "file_name": "driver.elf",
      "id": "scene-host",
      "provides": [
        {
          "api": 1,
          "capability": "ui.scene"
        }
      ],
      "requires": [
        {
          "api": 1,
          "capability": "display.output"
        },
        {
          "api": 1,
          "capability": "input.touch.raw"
        },
        {
          "api": 1,
          "capability": "platform.clock"
        },
        {
          "api": 1,
          "capability": "ui.presentation-profile"
        }
      ],
      "type": "driver",
      "version": "0.4.15"
    },
    "native_metadata": {
      "firmware_sha256": "d55b65af993adb40018ed5d8f990f8cedb26382ba612e5bd7f13c74395d66313",
      "firmware_size": 1634624,
      "layout": "riscrte-paired-appdata-v3",
      "product": "twatch-s3",
      "runtime_version": "0.4.20",
      "schema": "riscrte.cohort",
      "schema_version": 1,
      "source_repo": "michaelrolphone-cmyk/RiscRTE-T-Watch-S3",
      "source_revision": "c69d7914935fccdc04749f7bfa429c74219bf88a",
      "store_abi": 3,
      "version": "1.1.48"
    }
  },
  "wave": {
    "app_row": {
      "grants": [
        {
          "api": 1,
          "capability": "display.output",
          "instance_id": 5
        },
        {
          "api": 1,
          "capability": "input.touch.raw",
          "instance_id": 6
        },
        {
          "api": 2,
          "capability": "rtc.clock",
          "instance_id": 8
        },
        {
          "api": 1,
          "capability": "alarm.service",
          "instance_id": 0
        },
        {
          "api": 1,
          "capability": "telemetry.broadcast",
          "instance_id": 0
        },
        {
          "api": 1,
          "capability": "contexts.service",
          "instance_id": 0
        },
        {
          "api": 1,
          "capability": "input.power-key",
          "instance_id": 0
        },
        {
          "api": 1,
          "capability": "power.sleep",
          "instance_id": 0
        },
        {
          "api": 1,
          "capability": "display.power",
          "instance_id": 0
        },
        {
          "api": 1,
          "capability": "board.battery",
          "instance_id": 4
        },
        {
          "api": 1,
          "capability": "storage.key-value",
          "instance_id": 1
        },
        {
          "api": 1,
          "capability": "net.wifi",
          "instance_id": 15
        },
        {
          "api": 1,
          "capability": "bluetooth.hci",
          "instance_id": 16
        },
        {
          "api": 1,
          "capability": "motion.accel",
          "instance_id": 7
        },
        {
          "api": 1,
          "capability": "bluetooth.hid",
          "instance_id": 0
        },
        {
          "api": 1,
          "capability": "storage.key-value.atomic",
          "instance_id": 1
        }
      ],
      "image_source": {
        "path": "Apps/ble_touchpad/ble_touchpad.elf",
        "volume_instance": 12
      },
      "manifest": "ble_touchpad.json"
    },
    "app_manifest": {
      "architecture": "xtensa-esp32s3",
      "display_name": "BLE Touchpad",
      "entry": "app_main",
      "file_name": "ble_touchpad.elf",
      "icon": "solid:f245",
      "id": "ble_touchpad",
      "requires": [
        {
          "api": 1,
          "capability": "display.output"
        },
        {
          "api": 1,
          "capability": "input.touch.raw"
        },
        {
          "api": 2,
          "capability": "rtc.clock"
        },
        {
          "api": 1,
          "capability": "alarm.service"
        },
        {
          "api": 1,
          "capability": "telemetry.broadcast"
        },
        {
          "api": 1,
          "capability": "contexts.service"
        },
        {
          "api": 1,
          "capability": "input.power-key"
        },
        {
          "api": 1,
          "capability": "power.sleep"
        },
        {
          "api": 1,
          "capability": "display.power"
        },
        {
          "api": 1,
          "capability": "board.battery"
        },
        {
          "api": 1,
          "capability": "storage.key-value"
        },
        {
          "api": 1,
          "capability": "net.wifi"
        },
        {
          "api": 1,
          "capability": "bluetooth.hci"
        },
        {
          "api": 1,
          "capability": "motion.accel"
        },
        {
          "api": 1,
          "capability": "bluetooth.hid"
        },
        {
          "api": 1,
          "capability": "storage.key-value.atomic"
        }
      ],
      "type": "application",
      "version": "0.1.18"
    },
    "scene_row": {
      "manifest": "ui-scene/manifest.json"
    },
    "scene_manifest": {
      "architecture": "xtensa-esp32s3",
      "driver_abi": 2,
      "file_name": "driver.elf",
      "id": "scene-host",
      "provides": [
        {
          "api": 1,
          "capability": "ui.scene"
        }
      ],
      "requires": [
        {
          "api": 1,
          "capability": "display.output"
        },
        {
          "api": 1,
          "capability": "input.touch.raw"
        },
        {
          "api": 1,
          "capability": "platform.clock"
        },
        {
          "api": 1,
          "capability": "ui.presentation-profile"
        }
      ],
      "type": "driver",
      "version": "0.4.15"
    },
    "native_metadata": {
      "firmware_sha256": "cb3985c2e40e93d0b9c49e82055bd578fdf716677aedf9b0afef52b6df4f2f66",
      "firmware_size": 1819248,
      "layout": "riscrte-paired-appdata-v3",
      "product": "waveshare-s3-touch-amoled-206",
      "runtime_version": "0.4.20",
      "schema": "riscrte.cohort",
      "schema_version": 1,
      "source_repo": "michaelrolphone-cmyk/RiscRTE-Wavshare-S3-Touch-AMOLED-2.06",
      "source_revision": "97ca6157aa9c3bba7c7fee3858aa7d0ff896d13b",
      "store_abi": 3,
      "version": "0.1.66"
    }
  },
  "x4": {
    "app_row": {
      "catalog_authority": True,
      "grants": [
        {
          "api": 1,
          "capability": "display.output",
          "instance_id": 3
        },
        {
          "api": 1,
          "capability": "input.touch.raw",
          "instance_id": 4
        },
        {
          "api": 1,
          "capability": "input.navigation",
          "instance_id": 6
        },
        {
          "api": 1,
          "capability": "board.battery",
          "instance_id": 7
        },
        {
          "api": 1,
          "capability": "runtime.realtime",
          "instance_id": 0
        },
        {
          "api": 1,
          "capability": "storage.key-value",
          "instance_id": 1
        },
        {
          "api": 2,
          "capability": "alarm.service",
          "instance_id": 0
        },
        {
          "api": 1,
          "capability": "bluetooth.hid",
          "instance_id": 0
        },
        {
          "api": 1,
          "capability": "telemetry.broadcast",
          "instance_id": 0
        }
      ],
      "image_source": {
        "path": "Apps/ble_touchpad/ble_touchpad.elf",
        "volume_instance": 9
      },
      "manifest": "ble_touchpad.json"
    },
    "app_manifest": {
      "architecture": "xtensa-esp32s3",
      "entry": "app_main",
      "file_name": "ble_touchpad.elf",
      "id": "ble_touchpad",
      "requires": [
        {
          "api": 1,
          "capability": "display.output"
        },
        {
          "api": 1,
          "capability": "input.touch.raw"
        },
        {
          "api": 1,
          "capability": "input.navigation"
        },
        {
          "api": 1,
          "capability": "board.battery"
        },
        {
          "api": 1,
          "capability": "runtime.realtime"
        },
        {
          "api": 1,
          "capability": "storage.key-value"
        },
        {
          "api": 2,
          "capability": "alarm.service"
        },
        {
          "api": 1,
          "capability": "bluetooth.hid"
        },
        {
          "api": 1,
          "capability": "telemetry.broadcast"
        }
      ],
      "runtime_lifecycle": "foreground",
      "type": "application",
      "version": "0.1.23"
    },
    "scene_row": {
      "image_source": {
        "internal_fallback": True,
        "path": "Drivers/scene-host/image-8f841c721085de220e16460e07b2751be3673487838ab4c6915bf7d5405d3122.elf",
        "volume_instance": 9
      },
      "manifest": "ui-scene/manifest.json"
    },
    "scene_manifest": {
      "architecture": "xtensa-esp32s3",
      "driver_abi": 2,
      "file_name": "driver.elf",
      "id": "scene-host",
      "provides": [
        {
          "api": 1,
          "capability": "ui.scene"
        }
      ],
      "requires": [
        {
          "api": 1,
          "capability": "display.output"
        },
        {
          "api": 1,
          "capability": "input.touch.raw"
        },
        {
          "api": 1,
          "capability": "input.navigation"
        },
        {
          "api": 1,
          "capability": "platform.clock"
        },
        {
          "api": 1,
          "capability": "ui.presentation-profile"
        }
      ],
      "type": "driver",
      "version": "0.4.15"
    },
    "native_metadata": {
      "firmware_sha256": "ae5206ac566f9651d693c9834b17a39173ff961f2f9e580e828931125cf6f101",
      "firmware_size": 1816128,
      "layout": "riscrte-paired-appdata-v2",
      "product": "xteink-x4-pro",
      "runtime_version": "0.4.18",
      "schema": "riscrte.cohort",
      "schema_version": 1,
      "source_repo": "michaelrolphone-cmyk/RiscRTE-XTEINK-X4-PRO",
      "source_revision": "e361e73de14314bb4b8d2fb3a24eb81e2390b0ee",
      "store_abi": 2,
      "version": "0.1.124"
    }
  }
}
def checked_download(name,root):
 row=BASELINE[name];path=root/name
 if not path.exists():
  with urllib.request.urlopen(row['url'],timeout=180) as response:path.write_bytes(response.read())
 data=path.read_bytes()
 if len(data)!=row['size'] or digest(data)!=row['sha256']:raise ValueError('Published baseline bytes differ '+name)
 return data
def read_zip(data):
 with zipfile.ZipFile(io.BytesIO(data)) as z:
  names=z.namelist()
  if len(names)!=len(set(names)):raise ValueError('Duplicate SD path')
  return {safe(n):z.read(n) for n in names if not n.endswith('/')}
def baseline(tools,output):
 output.mkdir(parents=True,exist_ok=False);downloads=output/'downloads';downloads.mkdir()
 sys.path.insert(0,str(tools.resolve()))
 from read_only_spiffs import read_image
 old_assembly=json.loads(checked_download('assembly.json',downloads))
 products={};inspection={}
 for lane in ('watch','wave','x4'):
  raw=checked_download('product-'+lane+'-store-store.bin',downloads)
  stored=read_image(raw,len(raw));before={n:decode_image(b) if b.startswith(MAGIC) else b for n,b in stored.items()}
  sd=read_zip(checked_download('product-'+lane+'-sd-sd-apps.zip',downloads))
  encoding=old_assembly['products'][lane].get('image_storage')
  if encoding:
   if {n:digest(b) for n,b in before.items()}!=encoding['canonical_files'] or {n:digest(b) for n,b in stored.items()}!=encoding['stored_files']:raise ValueError('Published envelope inventory differs')
  boot=json.loads(before['boot.json'])
  app_rows=[r for r in boot['app_capabilities'] if json.loads(before[r['manifest']]).get('file_name')=='ble_touchpad.elf']
  scene_rows=[r for r in boot['drivers'] if any(c['capability']=='ui.scene' for c in json.loads(before[r['manifest']]).get('provides',[]))]
  if len(app_rows)!=1 or len(scene_rows)!=1:raise ValueError('Touchpad/scene authority is not unique '+lane)
  inspection[lane]={'store_size':len(raw),'payload_size':sum(map(len,stored.values())),'app_row':app_rows[0],'app_manifest':json.loads(before[app_rows[0]['manifest']]),'scene_row':scene_rows[0],'scene_manifest':json.loads(before[scene_rows[0]['manifest']]),'internal_touchpad':[(n,len(b),digest(b)) for n,b in before.items() if 'touchpad' in n],'sd_touchpad':[(n,len(b),digest(b)) for n,b in sd.items() if 'touchpad' in n],'providers':len(boot['drivers']),'applications':len(boot['app_capabilities']),'native_metadata':json.loads(before['cohort.json']) if 'cohort.json' in before else None}
  write_tree(output/lane/'before',before);write_tree(output/lane/'before-stored',stored);write_tree(output/lane/'before-sd',sd)
  products[lane]=(before,stored,sd,len(raw))
 (output/'inspection.json').write_bytes(encode(inspection))
 print(json.dumps(inspection,sort_keys=True,indent=2),flush=True)
 return products

def assemble(products,application,providers,tools,output):
 from current_bootfs import build
 from read_only_spiffs import read_image
 app=(application/'ble_touchpad.elf').read_bytes()
 common=json.loads((application/'ble_touchpad.json').read_text())
 builds=json.loads((application.parents[1]/'packages.json').read_text())['packages']
 if len(builds)!=1:raise ValueError('Only canonical Touchpad build expected')
 app_build=builds[0]
 if not app.startswith(b'\x7fELF') or app_build['sha256']!=digest(app) or not app_build['reproducibility']['matches']:raise ValueError('Canonical application build differs')
 if common['requires'] or common['id']!='ble_touchpad' or common['version']!='0.2.0':raise ValueError('Complete optional Touchpad manifest required')
 if any(r['sha256']!=digest(app) for r in app_build['selections'].values()):raise ValueError('Application selections differ')
 results={}
 for lane,(before,old_stored,before_sd,size) in products.items():
  spec=AUTHORITY[lane];out=dict(before);sd=dict(before_sd);boot=json.loads(before['boot.json'])
  row=copy.deepcopy(spec['app_row']);name=row['manifest'];scene_row=spec['scene_row'];scene_name=scene_row['manifest']
  if row not in boot['app_capabilities'] or scene_row not in boot['drivers']:raise ValueError('Authority changed '+lane)
  if json.loads(before[name])!=spec['app_manifest'] or json.loads(before[scene_name])!=spec['scene_manifest']:raise ValueError('Manifest baseline changed '+lane)
  if json.loads(before['cohort.json'])!=spec['native_metadata']:raise ValueError('Native metadata changed '+lane)
  # Only physical presentation/input authority moves into the already installed
  # scene service. Its provider row, dependencies and physical bindings stay exact.
  physical={'display.output','input.touch.raw','input.navigation'}
  delegated=[g for g in row['grants'] if g['capability'] in physical]
  grants=[g for g in row['grants'] if g['capability'] not in physical]
  grants.append({'capability':'ui.scene','api':1,'instance_id':0})
  originals=[g for g in grants if g['capability']=='alarm.service']
  if len(originals)!=1:raise ValueError('Original alarm authority ambiguous '+lane)
  old_alarm=originals[0]
  facade_rows=[]
  for r in boot['drivers']:
   m=json.loads(before[r['manifest']])
   if {'capability':'alarm.service.facade','api':2} in m.get('provides',[]):facade_rows.append(r)
  if len(facade_rows)!=1 or facade_rows[0].get('instance_id',0)!=0 or old_alarm not in facade_rows[0].get('bindings',[]):
   raise ValueError('Existing alarm facade must retain original alarm binding '+lane+' '+json.dumps(facade_rows))
  facade={'capability':'alarm.service.facade','api':2,'instance_id':0}
  grants[grants.index(old_alarm)]=facade
  declared={(r['capability'],r['api']) for r in common['optional']}
  if len(grants)>16 or any((g['capability'],g['api']) not in declared for g in grants):raise ValueError('Application grant capacity/declaration mismatch '+lane)
  proposed=copy.deepcopy(row);proposed['grants']=grants
  boot['app_capabilities'][boot['app_capabilities'].index(row)]=proposed
  installed=copy.deepcopy(spec['app_manifest'])
  for k in ('requires','optional','entry','architecture','version'):installed[k]=copy.deepcopy(common[k])
  out[name]=encode(installed)
  route=row.get('image_source')
  app_path=safe(route['path']) if route else safe(str(PurePosixPath(name).parent/installed['file_name']))
  target=sd if route else out
  if app_path not in target:raise ValueError('Original application route absent')
  target[app_path]=app
  sd_manifest=str(PurePosixPath(app_path).parent/'manifest.json') if route else None
  if sd_manifest:
   if sd_manifest not in sd or json.loads(sd[sd_manifest])!=spec['app_manifest']:raise ValueError('Original SD sidecar differs')
   sd[sd_manifest]=out[name]
  selected={'watch':'watch-compact','wave':'color-dynamic','x4':'full'}[lane]
  package=providers/selected/'scene-host'
  manifest=(package/'manifest.json').read_bytes()
  if not (package/'manifest.json').exists():raise ValueError('Selected provider manifest absent')
  new_provider=json.loads(manifest);previous=spec['scene_manifest']
  if {k:v for k,v in previous.items() if k!='version'}!={k:v for k,v in new_provider.items() if k!='version'} or new_provider['version']!='0.4.16':raise ValueError('Provider contract/profile changed '+lane)
  provider=(package/'driver.elf').read_bytes();pbuild=json.loads((package/'build.json').read_text())
  if pbuild['sha256']!=digest(provider) or pbuild['repeat_sha256']!=digest(provider) or pbuild['host_profile']!=selected:raise ValueError('Provider repeat binding differs')
  provider_path=str(PurePosixPath(scene_name).parent/'driver.elf')
  out[scene_name]=manifest;out[provider_path]=provider
  source=scene_row.get('image_source')
  if source:
   if not source.get('internal_fallback') or source['path'] not in sd:raise ValueError('Existing provider SD fallback missing')
   sd[source['path']]=provider
  out['boot.json']=encode(boot)
  changed={name,scene_name,provider_path,'boot.json'}
  if not route:changed.add(app_path)
  sd_changed=set([app_path,sd_manifest]) if route else set()
  if source:sd_changed.add(source['path'])
  if set(out)!=set(before) or set(sd)!=set(before_sd):raise ValueError('File inventory additions/removals forbidden')
  if any(out[n]!=b for n,b in before.items() if n not in changed):raise ValueError('Unrelated internal bytes changed')
  if any(sd[n]!=b for n,b in before_sd.items() if n not in sd_changed):raise ValueError('Unrelated SD bytes changed')
  unchanged_boot=copy.deepcopy(boot);unchanged_boot['app_capabilities'][unchanged_boot['app_capabilities'].index(proposed)]=row
  if unchanged_boot!=json.loads(before['boot.json']):raise ValueError('Unrelated authority changed')
  # Preserve the exact storage representation of every unrelated file.
  stored=dict(old_stored);encoded={}
  for n,b in out.items():
   if b!=before[n]:stored[n]=encode_image(b) if old_stored[n].startswith(MAGIC) else b
   if stored[n].startswith(MAGIC):encoded[n]=envelope_receipt(b,stored[n])
  if {n:decode_image(b) if b.startswith(MAGIC) else b for n,b in stored.items()}!=out:raise ValueError('Canonical recovery differs')
  if any(stored[n]!=b for n,b in old_stored.items() if n not in changed):raise ValueError('Unrelated stored bytes changed')
  for which,entries in (('after',out),('stored',stored),('after-sd',sd)):
   (output/lane/which).mkdir(parents=True,exist_ok=True);write_tree(output/lane/which,entries)
  native={}
  for kind in ('descriptor','qualification','firmware'):
   filename='native-'+lane+'-'+kind+('.bin' if kind=='firmware' else '.json')
   data=checked_download(filename,output/'downloads');native[kind]=BASELINE[filename]
   if kind=='firmware':
    meta=spec['native_metadata']
    if len(data)!=meta['firmware_size'] or digest(data)!=meta['firmware_sha256'] or ('RISC_RUNTIME_VERSION:'+meta['runtime_version']).encode()+b'\0' not in data:raise ValueError('Native firmware binding differs')
  result={'inventory_sha256':fingerprint(inventory(before,before_sd)),'touchpad_sha256':digest(app),
    'installed_manifest':name,'application_route':{'storage':'sd' if route else 'internal','path':app_path},
    'scene_profile':selected,'scene_sha256':digest(provider),'original_app_row':row,'app_row':proposed,
    'delegated_physical_grants':delegated,'alarm_substitution':{'from':old_alarm,'to':facade,'provider':facade_rows[0]},
    'changed_internal':sorted(n for n in changed if out[n]!=before[n]),'changed_sd':sorted(n for n in sd_changed if sd[n]!=before_sd[n]),
    'preserved_internal_files':len(out)-len(changed),'preserved_sd_files':len(sd)-len(sd_changed),
    'native_metadata_sha256':digest(out['cohort.json']),'native_firmware':native,'native_firmware_unchanged':True,
    'target_execution':False,'hardware':'not tested'}
  if encoded:result['image_storage']={'schema':1,'format':'RTEIMG1','images':encoded,'canonical_files':{n:digest(b) for n,b in sorted(out.items())},'stored_files':{n:digest(b) for n,b in sorted(stored.items())},'canonical_recovery_verified':True}
  result.update(pack_candidate(output/lane,stored,sd,size,build,read_image))
  results[lane]=result
  print('CUMULATIVE '+lane+' '+json.dumps(result,sort_keys=True),flush=True)
 (output/'assembly.json').write_bytes(encode({'schema':1,'products':results,'byte_identical_touchpad':digest(app),'packed':True,'baseline':BASELINE}))
 return results

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('bootfs-tools','output'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--inspect-only',action='store_true');p.add_argument('--application',type=Path);p.add_argument('--providers',type=Path);a=p.parse_args()
 products=baseline(a.bootfs_tools,a.output)
 if not a.inspect_only:
  if not a.application or not a.providers:p.error('--application and --providers required')
  assemble(products,a.application,a.providers,a.bootfs_tools,a.output)
