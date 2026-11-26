#!/usr/bin/env python3
"""Build a rear variant from the pinned front WebSocket camera package."""

import argparse
import io
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile

from check_ota_image import check_image
from patch_bno_library import apply as patch_bno_library
from patch_lidar_library import apply as patch_lidar_library


PACKAGE = Path(__file__).resolve().parent
FRONT = "1970ddf516ad548173c29f1e37ce4d8899eb274c"
FRONT_PATH = "firmware/esp32cam-rtsp-radio"
BOARD = "esp32cam_s3_wroom_n16r8"
VERSION = "1.0-rear.34-front-base"
REAR_VIDEO_PRIORITY_VERSION = "1.0-rear.35-front-cs5"


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one front adaptation point for {old!r}")
    return text.replace(old, new)


def edit(path, replacements):
    text = path.read_text()
    for old, new in replacements:
        text = replace(text, old, new)
    path.write_text(text)


def materialize_front(output):
    repository = PACKAGE.parents[1]
    if subprocess.run(
        ["git", "-C", repository, "cat-file", "-e", f"{FRONT}^{{commit}}"],
        stderr=subprocess.DEVNULL,
    ).returncode:
        subprocess.run(
            ["git", "-C", repository, "fetch", "--no-tags", "--depth=1", "origin", FRONT],
            check=True,
        )
    archive = subprocess.check_output(["git", "-C", repository, "archive", f"{FRONT}:{FRONT_PATH}"])
    with tempfile.TemporaryDirectory(prefix="rear-front-source-") as directory:
        package = Path(directory)
        with tarfile.open(fileobj=io.BytesIO(archive)) as tree:
            for member in tree.getmembers():
                relative = Path(member.name)
                if relative.is_absolute() or ".." in relative.parts or (
                    not member.isfile() and not member.isdir()
                ):
                    raise ValueError(f"Unsafe front package entry {member.name!r}")
                if member.isfile():
                    path = package / relative
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(tree.extractfile(member).read())
        subprocess.run([sys.executable, package / "prepare.py", "--output", output], check=True)


def install_rear_modules(output):
    for filename in ("ahrs_telemetry.cpp", "ultrasonic_telemetry.cpp"):
        (output / "src" / filename).unlink()
    paths = (
        "include/bno_telemetry.h", "include/radio_telemetry.h", "include/rear_health.h",
        "include/rear_ota_datagrams.h", "include/rear_ota_recovery.h", "include/rear_ranging.h",
        "partitions/rear-ota.csv", "src/bno_telemetry.cpp", "src/radio_telemetry.cpp",
        "src/rear_ota_datagrams.cpp", "src/rear_ota_recovery.cpp", "src/rear_ranging.cpp",
    )
    for relative in paths:
        target = output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(PACKAGE / "overlay" / relative, target)

    # Preserve the rear status API. Only the front Wi-Fi startup owns the radio.
    edit(output / "src/rear_ranging.cpp", [
        ("    if (station) {", "    if (!FRONT_WIFI_OWNS_RADIO && station) {"),
        ('    output["tx_power_requested_dbm"] = TX_POWER_QUARTER_DBM / 4.0;',
         '    if (FRONT_WIFI_OWNS_RADIO) output["tx_power_requested_dbm"] = nullptr;\n'
         '    else output["tx_power_requested_dbm"] = TX_POWER_QUARTER_DBM / 4.0;'),
        ('    output["power_save_requested"] = static_cast<int>(WIFI_PS_NONE);',
         '    if (FRONT_WIFI_OWNS_RADIO) output["power_save_requested"] = nullptr;\n'
         '    else output["power_save_requested"] = static_cast<int>(WIFI_PS_NONE);'),
    ])
    edit(output / "include/sensor_push.h", [
        ("void offerCamera(", "void pause(bool paused); void offerCamera("),
    ])
    edit(output / "src/sensor_push.cpp", [
        ('#include "radio_telemetry.h"', '#include "radio_telemetry.h"\n#include "bno_telemetry.h"'),
        ('#define CAMERA_SOCKET_ROLE "front"', '#define CAMERA_SOCKET_ROLE "rear"'),
        ('    packet["schema"]="biome.sensor_tick.v1";',
         '    packet["camera_id"]=CAMERA_SOCKET_ROLE;\n    packet["schema"]="biome.sensor_tick.v1";'),
        ('bool sensorPaused() { return false; }',
         'std::atomic<bool> paused{false};\nbool sensorPaused() { return paused.load(); }'),
        ('    if(kind=="clock") {',
         '''    if (kind=="bno") {
        StaticJsonDocument<512> reply;
        bno_telemetry::enqueueCommand(request.as<JsonObjectConst>(),reply.to<JsonObject>());
        reply["type"]="bno";
        String encoded;
        serializeJson(reply,encoded);
        socket.sendTXT(encoded);
    }
    if(kind=="clock") {'''),
        ('void offerCamera(uint32_t intervalMs)', 'void pause(bool value) { paused.store(value); }\n\n'
         'void offerCamera(uint32_t intervalMs)'),
    ])


def configure(output, version):
    settings = output / "include/settings.h"
    values = {
        "APP_TITLE": '"ROVER-REAR-CAMERA"',
        "APP_VERSION": json.dumps(version),
        "WIFI_SSID": '"Rover-Rear-Camera"',
        "DEFAULT_FRAME_SIZE": '"HD (1280x720)"',
        "DEFAULT_FRAME_DURATION": "200",
    }
    text = settings.read_text()
    for key, value in values.items():
        match = re.findall(rf"^#define {key} .+$", text, re.MULTILINE)
        if len(match) != 1:
            raise ValueError(f"Expected one camera setting {key}")
        text = replace(text, match[0], f"#define {key} {value}")
    settings.write_text(text)

    board = output / "boards" / f"{BOARD}.json"
    edit(board, [("ARDUINO_USB_CDC_ON_BOOT=1", "ARDUINO_USB_CDC_ON_BOOT=0")])
    edit(output / "platformio.ini", [
        ("build_flags =\n", "board_build.partitions = partitions/rear-ota.csv\n\n"
         "build_flags =\n  -D FRONT_WIFI_OWNS_RADIO=1\n  -D VL53L5CX_NB_TARGET_PER_ZONE=1\n"),
        ("lib_deps =\n", "lib_deps =\n  sparkfun/SparkFun BNO08x Cortex Based IMU@1.0.6\n"
         "  sparkfun/SparkFun VL53L5CX Arduino Library@1.0.3\n"),
    ])
    edit(output / "src/main.cpp", [
        ('#include "sensor_push.h"', '#include "sensor_push.h"\n#include "rear_ota_recovery.h"\n'
         '#include <Preferences.h>'),
        ("esp_err_t camera_init_result = ESP_FAIL;", "esp_err_t camera_init_result = ESP_FAIL;\n"
         "static bool otaRecoveryMode = false;"),
        ("  WiFi.disconnect(false, true);\n  ESP.restart();", "  ESP.restart();"),
        ("  setup_ota();\n  Serial0.printf", "  if (!rear_ota_recovery::enrolled()) setup_ota();\n"
         "  Serial0.printf"),
        ("  ArduinoOTA.setPassword(OTA_PASSWORD);", "  ArduinoOTA.setPassword(OTA_PASSWORD);\n"
         "  ArduinoOTA.setTimeout(10000);"),
        ("  iotWebConf.setConfigSavedCallback(on_config_saved);\n", ""),
        ("  iotWebConf.init();", '''  const bool savedConfigLoaded = iotWebConf.init();
  otaRecoveryMode = rear_ota_recovery::begin(thingName.c_str(),
      iotWebConf.getWifiSsidParameter()->valueBuffer,
      iotWebConf.getWifiPasswordParameter()->valueBuffer);
  if (otaRecoveryMode) return;
  Preferences cameraProfile;
  if (savedConfigLoaded && cameraProfile.begin("rear-camera", false)) {
    if (!cameraProfile.getBool("profile-v1", false)) {
      if (strcmp(param_frame_size.value(), DEFAULT_FRAME_SIZE) != 0 ||
          param_frame_duration.value() != DEFAULT_FRAME_DURATION) {
        strlcpy(param_frame_size.value(), DEFAULT_FRAME_SIZE, sizeof(param_frame_size.value()));
        param_frame_duration.value() = DEFAULT_FRAME_DURATION;
        iotWebConf.saveConfig();
      }
      cameraProfile.putBool("profile-v1", true);
    }
    cameraProfile.end();
  }
  iotWebConf.setConfigSavedCallback(on_config_saved);'''),
        ("void loop()\n{", "void loop()\n{\n  if (otaRecoveryMode) {\n"
         "    rear_ota_recovery::service();\n    return;\n  }"),
        ("  ArduinoOTA.handle();", "  if (!rear_ota_recovery::enrolled()) ArduinoOTA.handle();"),
    ])


def use_uart_console(output):
    # Arduino exposes UART0 as Serial when the rear USB CDC boot flag is off.
    main = output / "src/main.cpp"
    edit(main, [
        ("  Serial0.begin(115200);\n  Serial0.println(\"[front-startup] setup entered\");\n"
         "  Serial.begin(115200);",
         "  Serial.begin(115200);\n  Serial.println(\"[front-startup] setup entered\");"),
        ("  Serial0.setDebugOutput(true);\n", ""),
    ])
    main.write_text(main.read_text().replace("Serial0.", "Serial."))
    edit(output / "src/sensor_push.cpp", [("Serial0.printf", "Serial.printf")])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--build", action="store_true")
    parser.add_argument("--rear-video-priority", action="store_true",
                        help="Apply the existing rear CS5 traffic class to camera traffic")
    args = parser.parse_args()
    output = args.output.expanduser().resolve()
    materialize_front(output)
    install_rear_modules(output)
    version = REAR_VIDEO_PRIORITY_VERSION if args.rear_video_priority else VERSION
    configure(output, version)
    use_uart_console(output)
    if args.rear_video_priority:
        edit(output / "src/camera_websocket.inc", [
            ("videoSocket.transportChanged(false);",
             "videoSocket.transportChanged(type == WStype_CONNECTED);"),
        ])
    if args.build:
        subprocess.run(["pio", "pkg", "install", "-d", output, "-e", BOARD], check=True)
        patch_bno_library(output)
        patch_lidar_library(output)
        subprocess.run(["pio", "run", "-d", output, "-e", BOARD], check=True)
        check_image(output, version)


if __name__ == "__main__":
    main()
