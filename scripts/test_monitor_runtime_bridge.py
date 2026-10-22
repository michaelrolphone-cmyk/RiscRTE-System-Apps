#!/usr/bin/env python3
"""Bounded host-only unchanged monitor through pinned production Runtime."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
BASE = "0dd03a6b65c4e643081d8a2d8e175f08b5c02c23"
RUNTIME = "6a3b3d46f5b2857eacc6e9863799dec1f466b036"
MODES = "normal select-second slow-frame reopen missing-grant wrong-grant close-retained call-overrun close-overrun".split()
SIZES = [(240, 240), (480, 800)]
DEFINES = ["PORTABLE_SERIAL_STREAMS", "PORTABLE_SERIAL_MONITOR_APP", "PORTABLE_NATIVE_CUSTODY_FENCE", "PORTABLE_CHECKED_STORAGE_RUNTIME", "PORTABLE_APP_LAUNCH_GUARD", "PORTABLE_APP_OWNS_TOUCH_CHROME"]
FIXTURE = ROOT / "test/native_apps/monitor_runtime"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git(root, *args):
    return subprocess.check_output(["git", "-C", str(root), *args], text=True).strip()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n")


def manifests(folder, mode, activation):
    devices = []
    drivers = []
    for instance in (41, 42):
        devices.append({"instance_id": instance, "chip": {"vendor": "fixture", "model": "serial", "revision": "unspecified"}, "compatible": "monitor-fixture,serial", "config_type": "gpio.bank", "config_version": 1,
                        "config": {"pins": [instance-36], "active_high": True, "pull_up": False, "debounce_us": 0, "long_press_us": 0, "click_min_us": 0}})
    write_json(folder / "board.json", {"schema": "riscrte.board-hardware", "schema_version": 1, "board_id": "software-monitor-fixture", "revision": "unspecified", "buses": [], "devices": devices})
    for name, capability, instance in [("display", "display.output", 0), ("touch", "input.touch.raw", 0), ("battery", "board.battery", 0), ("serial-41", "serial.port", 41), ("serial-42", "serial.port", 42)]:
        manifest = {"type": "driver", "id": "monitor-"+name, "version": "1.0.0", "driver_abi": 2, "architecture": "xtensa-esp32s3", "file_name": name+".elf", "requires": [], "provides": [{"capability": capability, "api": 1}]}
        selected = {"manifest": name+".json"}
        if instance:
            manifest["requires"] = [{"capability": "hardware.device", "api": 1}]
            manifest["hardware_compatibility"] = [{"compatible": "monitor-fixture,serial", "revisions": ["unspecified"], "config_type": "gpio.bank", "config_version": 1}]
            selected["instance_id"] = instance
        write_json(folder / (name+".json"), manifest)
        drivers.append(selected)
    requirements = [{"capability": cap, "api": 1} for cap in ("display.output", "input.touch.raw", "board.battery")]
    write_json(folder / "app.json", {"type": "application", "id": "monitor-runtime-fixture", "version": "1.0.0", "architecture": "xtensa-esp32s3", "file_name": "default.elf", "entry": "app_main", "requires": requirements, "optional": [{"capability": "serial.port", "api": 1}]})
    grants = [dict(item, instance_id=0) for item in requirements]
    if mode != "missing-grant":
        grants.append({"capability": "serial.port", "api": 1, "instance_id": 42 if mode in ("wrong-grant", "select-second") else 41})
    write_json(folder / "boot.json", {"board": "board.json", "default_app": "default.elf", "provider_activation": activation, "drivers": drivers, "app_capabilities": [{"manifest": "app.json", "grants": grants}]})


def verify_render(folder, mode, width, height):
    # Compare all pixels of an exact glyph rectangle to the production font.
    # This is stronger than counting frames or asserting a nonblank screenshot.
    from PIL import Image
    font = {}
    for values, code in re.findall(r"\{([0-9,]+)\}, /\* (\d+) \*/", (ROOT / "Apps/serial_monitor_font.inc").read_text()):
        font[int(code)] = list(map(int, values.split(',')))
    assert len(font) == 95
    expected = "RX COMPLETE"
    matches = []
    captures = []
    for path in sorted(folder.glob("frame-*.ppm")):
        with Image.open(path) as image:
            image.load()
            assert image.size == (width, height)
            pixels = image.convert("RGB")
            correct = all(pixels.getpixel((8+i*8+x, 70+y)) == ((0, 0, 0) if font[ord(c)][y] & (1 << (7-x)) else (255, 255, 255))
                          for i, c in enumerate(expected) for y in range(12) for x in range(8))
            if correct:
                matches.append(path.name)
            png = path.with_suffix(".png")
            pixels.save(png)
            captures.append({"name": png.name, "sha256": sha(png), "ppm_sha256": sha(path)})
    assert captures, folder
    requires_rx = mode not in ("missing-grant", "wrong-grant", "call-overrun")
    assert bool(matches) == requires_rx, (mode, "rendered RX COMPLETE", matches)
    return {"rx_text_frames": matches, "captures": captures}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--runtime", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--sanitize", action="store_true")
    p.add_argument("--development", action="store_true", help="Allow dirty new suite only; never produces qualification PASS")
    p.add_argument("--case", choices=MODES, help="Development-only one-mode smoke")
    a = p.parse_args()
    assert not a.case or a.development, "Subset runs are not qualification"
    runtime, out = a.runtime.resolve(), a.output.resolve()
    assert not out.is_relative_to(ROOT) and not out.is_relative_to(runtime), "Output must be outside source trees"
    out.mkdir(parents=True, exist_ok=False)
    # Reserve a valid external output directory before touching source/staging.
    # Even a failed git check, copied header, compiler or test leaves a receipt.
    write_json(out/"evidence.json", {"result": "UNQUALIFIED", "phase": "preflight",
        "expected_cases": 36, "cases": [], "requested_runtime": str(runtime),
        "development": a.development, "sanitized": a.sanitize})
    try:
        execute(a, runtime, out)
    except BaseException as error:
        receipt = json.loads((out/"evidence.json").read_text())
        receipt.update(result="UNQUALIFIED", failure={"type": type(error).__name__, "message": str(error)})
        write_json(out/"evidence.json", receipt)
        raise


def execute(a, runtime, out):
    system_sha = git(ROOT, "rev-parse", "HEAD")
    assert git(runtime, "rev-parse", "HEAD") == RUNTIME
    assert not git(runtime, "status", "--porcelain"), "Runtime must be clean"
    subprocess.run(["git", "-C", str(ROOT), "merge-base", "--is-ancestor", BASE, system_sha], check=True)
    status = git(ROOT, "status", "--porcelain")
    assert a.development or not status, "System must be clean"
    # No production or pre-existing suite/evidence differences from exact PR157.
    allowed = {"scripts/test_monitor_runtime_bridge.py", ".github/workflows/monitor-runtime-bridge.yml", "docs/MONITOR_RUNTIME_BRIDGE.md"}
    changed = git(ROOT, "diff", "--name-only", BASE).splitlines()
    assert all(path in allowed or path.startswith("test/native_apps/monitor_runtime/") for path in changed), changed
    initial = json.loads((out/"evidence.json").read_text())
    initial.update(phase="staging", system=system_sha, runtime=RUNTIME, system_status=status.splitlines())
    write_json(out/"evidence.json", initial)
    stage, build = out / "include", out / "build"
    shutil.copytree(ROOT / "lib/PortableApps/include", stage)
    build.mkdir()
    canonical = {}
    for group in ("app", "driver", "hardware"):
        for source in sorted((runtime / "sdk" / group).glob("*.h")):
            data = source.read_bytes()
            canonical[str(source.relative_to(runtime))] = sha(source)
            (stage / source.name).write_bytes(data)
    env = dict(os.environ, PYTHONDONTWRITEBYTECODE="1", ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    san = ["-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer"] if a.sanitize else []
    common = ["-O1", "-g", "-Wall", "-Wextra", "-Werror", *san]
    includes = ["-I"+str(stage), "-I"+str(ROOT/"lib/NativeApps/include"), "-I"+str(FIXTURE)]
    cc, cxx = os.environ.get("CC", "cc"), os.environ.get("CXX", "c++")
    flags = ["-std=c11", *common, "-fPIC", "-fvisibility=hidden", *includes]
    defines = ["-D"+name for name in DEFINES]
    commands, results = [], []
    receipt = {"result": "UNQUALIFIED", "phase": "build-and-test", "system": system_sha, "base": BASE, "runtime": RUNTIME,
               "system_status": status.splitlines(), "system_tree": git(ROOT, "rev-parse", "HEAD^{tree}"), "runtime_tree": git(runtime, "rev-parse", "HEAD^{tree}"), "sanitized": a.sanitize, "development": a.development,
               "expected_cases": 36, "runtime_headers": canonical, "commands": commands, "cases": results,
               "sanitizer_environment": {name: env[name] for name in ("ASAN_OPTIONS", "UBSAN_OPTIONS")}}
    log_path = out / "tests.log"
    with log_path.open("w") as log:
        def run(command):
            command = list(map(str, command)); commands.append(command)
            log.write("$ "+" ".join(command)+"\n"); log.flush()
            subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, env=env, check=True, timeout=120)
        try:
            for name in ("display", "touch", "battery"):
                run([cc, *flags, "-shared", FIXTURE/(name+"_provider.c"), "-o", build/(name+".elf")])
            for instance in (41, 42):
                run([cc, *flags, "-shared", "-DFIXTURE_SERIAL_INSTANCE="+str(instance), FIXTURE/"serial_provider.c", "-o", build/("serial-"+str(instance)+".elf")])
            run([cc, *flags, *defines, "-Dapp_module_init=monitor_real_init", "-Dapp_module_fini=monitor_real_fini", "-c", ROOT/"lib/PortableApps/src/adapter.c", "-o", build/"adapter.o"])
            run([cc, *flags, *defines, "-Dapp_main=monitor_real_main", "-c", ROOT/"Apps/serial_monitor.c", "-o", build/"monitor.o"])
            run([cc, *flags, *defines, "-shared", build/"adapter.o", build/"monitor.o", ROOT/"lib/PortableApps/src/PortableSerialClient.c", FIXTURE/"app_module.c", "-o", build/"default.elf"])
            runtime_sources = ["src/bootstrap/Json.cpp", "src/bootstrap/Board.cpp", "src/bootstrap/Runtime.cpp", "src/runtime/streams/ProviderQueueHost.cpp", "src/runtime/streams/AppStreamSessions.cpp", "src/runtime/drivers/ProviderGraphV2.cpp", "src/runtime/drivers/ProviderModuleV2.cpp"]
            # Runtime host matches the pinned upstream runner optimization (O0).
            # Keep strict warnings and ASan/UBSan; do not suppress the O1
            # ArduinoJson maybe-uninitialized diagnostic or alter its source.
            run([cxx, "-std=c++17", *common, "-O0", "-Wno-missing-field-initializers", "-no-pie", "-rdynamic", "-DRISC_STREAM_HOST_TESTING",
                 *["-I"+str(runtime/part) for part in ("src", "sdk/app", "sdk/driver", "sdk/hardware", "lib/ArduinoJson/src", "test/drivers/stubs", "test")], "-I"+str(FIXTURE),
                 *[runtime/path for path in runtime_sources], FIXTURE/"runtime_test.cpp", "-ldl", "-pthread", "-o", build/"test"])
            # A symbol boundary gate prevents accidentally inheriting the fake Runtime fixture.
            dynamic = subprocess.check_output(["nm", "-D", build/"default.elf"], text=True)
            assert re.search(r" U risc_runtime_get_api$", dynamic, re.M)
            assert not re.search(r" [TW] risc_runtime_get_api$", dynamic, re.M)
            for symbol in ("app_main", "app_module_init", "app_module_fini"):
                assert re.search(r" T "+symbol+r"$", dynamic, re.M), symbol
            (out/"app-symbols.txt").write_text(dynamic)
            for activation in ("eager", "demand"):
                for mode in ([a.case] if a.case else MODES):
                    for width, height in SIZES:
                        case = out / (mode+"-"+activation+"-"+str(width)+"x"+str(height)); case.mkdir()
                        for module in build.glob("*.elf"):
                            shutil.copyfile(module, case/module.name)
                        manifests(case, mode, activation)
                        run([build/"test", case, mode, activation, width, height, case])
                        result = json.loads((case/"result.json").read_text())
                        result.update(verify_render(case, mode, width, height))
                        result["directory"] = case.name
                        write_json(case/"result.json", result)
                        results.append(result)
            assert a.development or len(results) == 36
            assert len({(r["mode"],r["activation"],r["width"],r["height"]) for r in results}) == len(results)
            assert git(ROOT, "status", "--porcelain") == status
            assert not git(runtime, "status", "--porcelain")
            receipt["result"] = "DEVELOPMENT_PASS" if a.development else "PASS"
            receipt["phase"] = "complete"
        finally:
            log.flush()
            receipt["log_sha256"] = sha(log_path)
            inputs = list(FIXTURE.glob("*")) + [ROOT/"Apps/serial_monitor.c", ROOT/"Apps/serial_monitor_font.inc", ROOT/"lib/PortableApps/src/adapter.c", ROOT/"lib/PortableApps/src/PortableSerialClient.c", Path(__file__).resolve()]
            inputs += [ROOT/path for path in git(ROOT, "ls-files", "lib/PortableApps/include", "lib/PortableApps/src", "lib/NativeApps/include").splitlines()]
            receipt["source_hashes"] = {str(path.relative_to(ROOT)): sha(path) for path in inputs if path.is_file()}
            receipt["runtime_source_hashes"] = {path: sha(runtime/path) for path in runtime_sources} if "runtime_sources" in locals() else {}
            write_json(out/"evidence.json", receipt)
    print(receipt["result"], "monitor through production Runtime:", len(results), "cases", out)

if __name__ == "__main__":
    main()
