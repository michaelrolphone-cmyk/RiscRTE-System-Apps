#!/usr/bin/env python3
"""Exercise the production native toolbar hook with host provider doubles.

Stages the exact canonical Runtime 602 header, tests ordinary and ASan/UBSan
paper/Quick profiles, and verifies compile/link refusals and pinned Xtensa
object compilation. No product builder, BIN publication, install or hardware.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
RUNTIME_REF = "602ae9bd618e13407b5b94bcad86cdabc23c99ea"
FLAGS = ["-DPORTABLE_NATIVE_TIME_TOOLBAR", "-DPORTABLE_NATIVE_CUSTODY_FENCE"]
QUICK = ["quick_actions.c", "quick_render.c", "quick_session.c"]
CASES = [[name] for name in ("runtime-short", "runtime-no-retain", "valid", "unavailable",
                           "gregorian", "recover-samples", "retained-false", "retained-true")]
CASES += [["malformed", str(i)] for i in range(17)]
QUICK_CASES = [[name] for name in ("quick-retained-false", "quick-retained-true", "quick-modal", "quick-modal-retained",
                                 "quick-actions", "quick-action-retained", "fini-status-retained", "fini-stop-retained")]


def run(command, **kwargs):
    return subprocess.run(list(map(str, command)), check=True, **kwargs)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def refused(command, expected, **kwargs):
    result = subprocess.run(list(map(str, command)), text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, **kwargs)
    assert result.returncode, "Unexpected successful build: " + " ".join(map(str, command))
    assert expected in result.stdout, result.stdout
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime-sdk", type=Path, default=ROOT.parent / "x4-runtime-provider-retention/sdk/app")
    parser.add_argument("--xtensa-cc", type=Path, default=ROOT.parent / "watch-build-tools/platformio-core/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "build/native-toolbar")
    parser.add_argument("--normal-only", action="store_true", help="Development diagnostic; final verification uses both builds")
    args = parser.parse_args()
    sdk = args.runtime_sdk.resolve()
    repo = sdk.parents[1]
    headers = ["RiscRuntimeV1.h", "RiscRealtimeV1.h"]
    for name in headers:
        canonical = subprocess.check_output(["git", "-C", repo, "show", RUNTIME_REF + ":sdk/app/" + name])
        assert canonical == (sdk / name).read_bytes(), name + " is not the pinned canonical header"
    bundled = ROOT / "lib/PortableApps/include/RiscRuntimeV1.h"
    bundled_before = bundled.read_bytes()
    cc = os.environ.get("CC", "cc")
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    source = ROOT / "test/native_apps/portable_native_toolbar_test.c"
    adapter = ROOT / "lib/PortableApps/src/adapter.c"
    tracked = [source, adapter, Path(__file__).resolve(), ROOT / "lib/PortableApps/include/PortableNativeTimeToolbar.h",
               ROOT / "lib/PortableApps/include/PortableNativeCustody.h",
               *[ROOT / "lib/PortableApps/src" / name for name in
                 ["native_custody_adapter.inc", "native_toolbar.inc", "foreground_adapter_open.inc", "nova.inc", "paper.inc", "quick_adapter.inc", *QUICK]]]
    source_hashes = {str(path.relative_to(ROOT)): sha(path) for path in tracked}
    receipt = {"purpose": "production generic native toolbar adapter and Quick hook; providers and app reader are doubles",
               "runtime_ref": RUNTIME_REF, "runtime_headers": {name: sha(sdk / name) for name in headers},
               "host_compiler": subprocess.check_output([cc, "--version"], text=True).splitlines()[0],
               "target_builder_run": False, "hardware_qualification": "not run",
               "quick_coverage": "direct production clock/action and modal loop paths; gesture controller not qualified",
               "runs": {}, "build_refusals": []}
    with tempfile.TemporaryDirectory(prefix="native-toolbar-") as temporary:
        stage = Path(temporary)
        include = stage / "include"
        shutil.copytree(ROOT / "lib/PortableApps/include", include)
        shutil.copytree(ROOT / "lib/PortableApps/time", stage / "time")
        for name in headers:
            shutil.copyfile(sdk / name, include / name)
        includes = ["-I" + str(include), "-I" + str(ROOT / "lib/NativeApps/include")]
        common = [cc, "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror"]
        environment = dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
        for quick in (False, True):
            profile = "paper-quick" if quick else "paper"
            profile_flags = ["-DTEST_NATIVE_TOOLBAR_QUICK"] if quick else []
            helpers = [ROOT / "lib/PortableApps/src" / name for name in QUICK] if quick else []
            cases = CASES + (QUICK_CASES if quick else [])
            for sanitized in ([False] if args.normal_only else [False, True]):
                label = profile + ("-asan-ubsan" if sanitized else "-normal")
                binary = out / label
                sanitizers = ["-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer", "-no-pie"] if sanitized else []
                run([*common, *sanitizers, *FLAGS, *profile_flags, *includes, source, *helpers,
                     "-Wl,--wrap=free", "-o", binary], timeout=120)
                results = []
                with (out / (label + ".log")).open("w") as log:
                    for case in cases:
                        result = run([binary, *case], env=environment, text=True, stdout=subprocess.PIPE, timeout=20)
                        log.write(result.stdout)
                        results.append({"arguments": case, "result": json.loads(result.stdout.strip()) if result.stdout.strip() else {"loader_refused": True}})
                receipt["runs"][label] = results
                print(f"Native toolbar {label}: {len(results)} fresh-process cases passed", flush=True)
        # A deployment must provide the private callback; there is no weak or
        # silent fallback to RTC when the generic profile is selected.
        log = refused([*common, *FLAGS, *includes, "-DTEST_NATIVE_TOOLBAR_MISSING_HOOK", source,
                       "-Wl,--wrap=free", "-o", stage / "missing-hook"], "portable_app_native_local_time", timeout=120)
        (out / "missing-hook.log").write_text(log)
        receipt["build_refusals"].append("missing app callback fails linkage")
        conflicts = ["PORTABLE_RTC_WALL_TIME", "PORTABLE_RTC_UTC8_DENVER", "PORTABLE_SETTINGS_NATIVE_TIME",
                     "PORTABLE_DESK_CLOCK_SPARSE_START", "PORTABLE_SETTINGS_APP", "PORTABLE_DESK_CLOCK"]
        for flag in conflicts:
            log = refused([*common, *FLAGS, "-D" + flag, *includes, "-c", adapter,
                           "-o", stage / "conflict.o"], "Native toolbar requires one unambiguous app-owned time source", timeout=120)
            (out / ("conflict-" + flag + ".log")).write_text(log)
            receipt["build_refusals"].append(flag)
        log = refused([*common, "-DPORTABLE_NATIVE_TIME_TOOLBAR", *includes, "-c", adapter,
                       "-o", stage / "no-fence.o"], "Native toolbar requires the canonical native custody fence", timeout=120)
        (out / "missing-fence.log").write_text(log)
        receipt["build_refusals"].append("missing custody flag")
        # The public compatibility header lacks the required complete suffix.
        legacy_include = stage / "legacy-include"
        shutil.copytree(include, legacy_include)
        shutil.copyfile(bundled, legacy_include / "RiscRuntimeV1.h")
        log = refused([*common, *FLAGS, "-I" + str(legacy_include), "-I" + str(ROOT / "lib/NativeApps/include"),
                       "-c", adapter, "-o", stage / "legacy-sdk.o"], "error:", timeout=120)
        (out / "legacy-sdk.log").write_text(log)
        receipt["build_refusals"].append("incomplete Runtime compatibility SDK")
        version = subprocess.check_output([args.xtensa_cc, "--version"], text=True).splitlines()[0]
        assert "8.4.0" in version and "2021r2-patch5" in version, version
        receipt["xtensa_compiler"] = version
        target_harness = stage / "target-harness.c"
        target_harness.write_text('''#include "PortableApps.h"
#include "PortableNativeTimeToolbar.h"
const t5_app_manifest_t portable_catalog[1]={{.compatible=false}};
const unsigned portable_catalog_count=0;
#ifndef OMIT_NATIVE_TOOLBAR_HOOK
bool portable_app_native_local_time(twatch_rtc_time_v1 *out) {
  *out=(twatch_rtc_time_v1){2026,10,7,3,13,42,56};return true;
}
#endif
__attribute__((visibility("default"))) void app_main(void){}
''')
        export_map = stage / "exports.map"
        export_map.write_text("{ global: app_main; app_module_init; app_module_fini; local: *; };\n")
        target_common = [args.xtensa_cc, "-std=c11", "-Os", "-fPIC", "-mtext-section-literals", "-mlongcalls",
                         "-fvisibility=hidden", "-ffreestanding", "-fno-builtin", "-Wall", "-Wextra", "-Werror"]
        for quick in (False, True):
            target = stage / ("toolbar-quick.o" if quick else "toolbar.o")
            profile_flags = ["-DPORTABLE_QUICK_ACTIONS", "-DPORTABLE_ALARM_CLIENT"] if quick else []
            run([*target_common,
                 *FLAGS, *profile_flags, *includes, "-c", adapter, "-o", target], timeout=120)
            symbols = subprocess.check_output([str(args.xtensa_cc).removesuffix("gcc") + "nm", target], text=True)
            assert any(line.split()[-2:] == ["U", "portable_app_native_local_time"] for line in symbols.splitlines())
            assert "np_rtc_grant" not in symbols and "fixture_time_forward" not in symbols
            target_helpers = [ROOT / "lib/PortableApps/src" / name for name in QUICK] if quick else []
            link = [*target_common, "-nostdlib", "-nostartfiles", "-shared", "-Wl,--hash-style=sysv",
                    "-Wl,--version-script=" + str(export_map), *FLAGS, *profile_flags, *includes,
                    target, target_harness, *target_helpers, "-lgcc"]
            binary = stage / ("toolbar-quick.elf" if quick else "toolbar.elf")
            run([*link, "-o", binary], timeout=120)
            dynamic = subprocess.check_output([str(args.xtensa_cc).removesuffix("gcc") + "nm", "-D", binary], text=True)
            exports = {line.split()[-1] for line in dynamic.splitlines() if len(line.split()) >= 3 and line.split()[-2] in {"T", "D", "B", "R"}}
            imports = {line.split()[-1] for line in dynamic.splitlines() if " U " in " " + line}
            assert exports == {"app_main", "app_module_init", "app_module_fini"}, exports
            assert imports <= {"risc_runtime_get_api", "memcpy", "memset", "memcmp", "strcmp", "strlen", "snprintf", "strcpy", "malloc", "free"}, imports
            log = refused([*link, "-DOMIT_NATIVE_TOOLBAR_HOOK", "-o", stage / "missing-target-hook.elf"],
                          "portable_app_native_local_time", timeout=120)
            label = "paper-quick" if quick else "paper"
            (out / (label + "-target-missing-hook.log")).write_text(log)
            receipt["build_refusals"].append(label + " target shared ELF missing callback")
        receipt["xtensa_adapter_harness_links"] = ["paper", "paper-quick"]
    assert bundled.read_bytes() == bundled_before, "Bundled Runtime header was changed"
    receipt["bundled_runtime_header_unchanged"] = True
    assert source_hashes == {str(path.relative_to(ROOT)): sha(path) for path in tracked}, "Source changed during qualification"
    receipt["source_sha256"] = source_hashes
    receipt["process_cases"] = sum(map(len, receipt["runs"].values()))
    evidence = out / "evidence.json"
    evidence.write_text(json.dumps(receipt, indent=2) + "\n")
    print(f"{receipt['process_cases']} cases, {len(receipt['build_refusals'])} build refusals and two pinned Xtensa harness links passed; evidence: {evidence}")


if __name__ == "__main__":
    main()
