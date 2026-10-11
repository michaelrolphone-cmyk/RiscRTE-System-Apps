#!/usr/bin/env python3
"""Actual native toolbar owner/adapter qualification; no product publication."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
from native_app_symbols import firmware_exports, validate_imports

ROOT = Path(__file__).resolve().parents[1]
RUNTIME_REF = "30dcec5ce6ce33223f2b203a2399283e1f758567"
BASE_REF = "81f884b8a053cf917054fb1433c7850714cd0c48"
FLAGS = ["-DPORTABLE_NATIVE_TIME_TOOLBAR", "-DPORTABLE_NATIVE_CUSTODY_FENCE"]
HELPERS = ["PortableNativeTimeSource.c", "PortableRealtimeClient.c", "PortableTimeZone.c",
           "PortableTimeZoneCatalog.c", "PortableTimeZonePreference.c"]
QUICK = ["quick_actions.c", "quick_render.c", "quick_session.c"]
CASES = ["valid", "zones-boundaries", "already-retained", "kv-missing", "kv-io", "kv-invalid", "kv-small",
         "kv-corrupt", "kv-size", "kv-context", "kv-unknown", "kv-retained", "kv-table-short", "kv-table-version",
         "kv-table-get", "kv-acquire-empty", "kv-acquire-dirty", "kv-grant", "kv-acquire-retained",
         "kv-release-false", "kv-release-dirty", "kv-release-size", "kv-release-retained", "native-missing",
         "native-io", "native-invalid", "native-unset", "native-invalid-validity", "native-size", "native-reserved",
         "native-bracket", "native-nanos", "native-resolution", "native-negative", "native-range",
         "native-context", "native-unknown", "native-retained", "native-table-short", "native-table-version",
         "native-table-read", "native-table-context", "native-acquire-dirty", "native-grant", "native-acquire-retained",
         "native-release-false", "native-release-dirty", "native-release-size", "native-release-retained"]


def run(command, **kwargs):
    return subprocess.run(list(map(str, command)), check=True, **kwargs)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--xtensa-cc", type=Path, default=ROOT.parent / "watch-build-tools/platformio-core/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "build/native-time-source")
    args = parser.parse_args()
    sdk = args.runtime.resolve() / "sdk/app"
    out = args.output_dir.resolve(); out.mkdir(parents=True, exist_ok=True)
    cc = os.environ.get("CC", "cc")
    helpers = [ROOT / "lib/PortableApps/src" / name for name in HELPERS]
    test = ROOT / "test/native_apps/portable_native_time_source_test.c"
    tracked = [*helpers, test, ROOT / "test/native_apps/portable_native_toolbar_test.c", Path(__file__).resolve(),
               *sorted((ROOT / "lib/PortableApps/include").glob("*.h")),
               *[ROOT / "lib/PortableApps/src" / name for name in
                 ["adapter.c", "native_toolbar.inc", "native_custody_adapter.inc", "foreground_adapter_open.inc",
                  "paper.inc", "nova.inc", "quick_adapter.inc", *QUICK]]]
    receipt = {"runtime_ref": RUNTIME_REF, "baseline_ref": BASE_REF, "host_cases": {}, "target": {},
               "source_sha256": {str(path.relative_to(ROOT)): sha(path) for path in tracked},
               "product_builders_changed": False, "hardware": "not run", "publication": "none"}
    runtime_source = subprocess.check_output(["git", "-C", args.runtime, "show", RUNTIME_REF + ":src/bootstrap/Runtime.cpp"])
    assert b'{"risc_runtime_get_api",reinterpret_cast<const void*>(&risc_runtime_get_api)}' in runtime_source
    receipt["runtime_export_source_sha256"] = hashlib.sha256(runtime_source).hexdigest()
    for name in ["RiscRuntimeV1.h", "RiscRealtimeV1.h"]:
        expected = subprocess.check_output(["git", "-C", args.runtime, "show", RUNTIME_REF + ":sdk/app/" + name])
        assert expected == (sdk / name).read_bytes(), name
        receipt[name] = sha(sdk / name)
    with tempfile.TemporaryDirectory(prefix="native-time-source-") as temporary:
        stage = Path(temporary)
        include = stage / "include"
        shutil.copytree(ROOT / "lib/PortableApps/include", include)
        shutil.copytree(ROOT / "lib/PortableApps/time", stage / "time")
        for name in ["RiscRuntimeV1.h", "RiscRealtimeV1.h"]:
            shutil.copyfile(sdk / name, include / name)
        includes = ["-I" + str(include), "-I" + str(ROOT / "lib/NativeApps/include")]
        environment = dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
        for quick in [False, True]:
            profile = "quick" if quick else "paper"
            profile_flags = ["-DTEST_NATIVE_TOOLBAR_QUICK"] if quick else []
            quick_helpers = [ROOT / "lib/PortableApps/src" / name for name in QUICK] if quick else []
            cases = CASES + (["quick-modal", "quick-modal-retained"] if quick else [])
            for sanitized in [False, True]:
                label = profile + ("-asan-ubsan" if sanitized else "-normal")
                sanitizers = ["-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer", "-no-pie"] if sanitized else []
                binary = out / label
                run([cc, "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror", *sanitizers,
                     *FLAGS, *profile_flags, *includes, test, *helpers, *quick_helpers, "-Wl,--wrap=free", "-o", binary])
                results = []
                with (out / (label + ".log")).open("w") as log:
                    for case in cases:
                        result = run([binary, case], env=environment, text=True, stdout=subprocess.PIPE, timeout=20)
                        log.write(result.stdout); results.append(json.loads(result.stdout))
                receipt["host_cases"][label] = results
                print(label + ": " + str(len(results)) + " actual owner/adapter cases passed", flush=True)
        version = subprocess.check_output([args.xtensa_cc, "--version"], text=True).splitlines()[0]
        assert "8.4.0" in version and "2021r2-patch5" in version, version
        receipt["target_compiler"] = version
        target_flags = ["-std=c11", "-Os", "-fPIC", "-mtext-section-literals", "-mlongcalls",
                        "-fvisibility=hidden", "-ffreestanding", "-fno-builtin", "-ffunction-sections", "-fdata-sections",
                        "-Wall", "-Wextra", "-Werror"]
        harness = stage / "harness.c"
        harness.write_text('''#include "PortableApps.h"
#ifdef PORTABLE_NATIVE_TIME_TOOLBAR
#include "PortableNativeTimeToolbar.h"
#endif
const t5_app_manifest_t portable_catalog[1]={{.compatible=false}};
const unsigned portable_catalog_count=0;
__attribute__((visibility("default"))) void app_main(void){
#ifdef PORTABLE_NATIVE_TIME_TOOLBAR
 twatch_rtc_time_v1 local; (void)portable_app_native_local_time(&local);
#endif
}
''')
        export_map = stage / "exports.map"
        export_map.write_text("{ global: app_main; app_module_init; app_module_fini; local: *; };\n")
        validator = stage / "validate"
        run([cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I" + str(ROOT / "test/native_apps/stubs"),
             "-I" + str(ROOT / "lib/elf_loader/include"), ROOT / "lib/elf_loader/src/esp_elf_validate.c",
             ROOT / "test/native_apps/validate_test.c", "-o", validator])
        for quick in [False, True]:
            profile = "quick" if quick else "paper"
            profile_flags = ["-DPORTABLE_QUICK_ACTIONS", "-DPORTABLE_ALARM_CLIENT"] if quick else []
            quick_helpers = [ROOT / "lib/PortableApps/src" / name for name in QUICK] if quick else []
            binary = out / (profile + ".elf")
            run([args.xtensa_cc, *target_flags, *FLAGS, *profile_flags, *includes, "-nostdlib", "-nostartfiles",
                 "-shared", "-Wl,--hash-style=sysv", "-Wl,--gc-sections", "-Wl,--version-script=" + str(export_map),
                 ROOT / "lib/PortableApps/src/adapter.c", harness, *helpers, *quick_helpers, "-lgcc", "-o", binary])
            symbols = subprocess.check_output([str(args.xtensa_cc).removesuffix("gcc") + "readelf", "--dyn-syms", "--wide", binary], text=True)
            imports = validate_imports(symbols, firmware_exports(ROOT) | {"risc_runtime_get_api"})
            assert imports <= {"risc_runtime_get_api", "memcpy", "memset", "memcmp", "strcmp", "strlen", "snprintf", "strcpy", "malloc", "free"}, imports
            exports = {line.split()[-1] for line in symbols.splitlines()
                       if len(line.split()) >= 8 and line.split()[4] == "GLOBAL" and line.split()[6] != "UND"}
            assert exports == {"app_main", "app_module_init", "app_module_fini"}, exports
            run([validator, binary], timeout=20)
            receipt["target"][profile] = {"imports": sorted(imports), "exports": sorted(exports),
                                           "elf_sha256": sha(binary), "loader_validation": "PASS"}
        # No builder activation and no flag-off target code or Watch ABI change.
        baseline = stage / "baseline"; baseline.mkdir()
        archive = subprocess.check_output(["git", "-C", ROOT, "archive", BASE_REF])
        with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
            tar.extractall(baseline, filter="data")
        receipt["watch_flag_off_sha256"] = {}
        for quick in [False, True]:
            profile = "watch-quick" if quick else "watch"
            flags = ["-DPORTABLE_QUICK_ACTIONS", "-DPORTABLE_ALARM_CLIENT"] if quick else []
            for label, repo in [("base", baseline), ("off", ROOT)]:
                target_include = ["-I" + str(repo / "lib/PortableApps/include"), "-I" + str(repo / "lib/NativeApps/include")]
                common = [args.xtensa_cc, *target_flags, *flags, *target_include]
                obj = stage / (profile + "-" + label + ".o")
                run([*common, "-c", repo / "lib/PortableApps/src/adapter.c", "-o", obj])
                quick_helpers = [repo / "lib/PortableApps/src" / name for name in QUICK] if quick else []
                binary = stage / (profile + "-" + label + ".elf")
                run([*common, "-nostdlib", "-nostartfiles", "-shared", "-Wl,--hash-style=sysv",
                     "-Wl,--version-script=" + str(export_map), obj, harness, *quick_helpers, "-lgcc", "-o", binary])
            for suffix in [".o", ".elf"]:
                before, after = [stage / (profile + "-" + label + suffix) for label in ["base", "off"]]
                assert before.read_bytes() == after.read_bytes(), profile + suffix
                receipt["watch_flag_off_sha256"][profile + suffix] = sha(after)
        empty_obj = stage / "flag-off-source.o"
        run([args.xtensa_cc, *target_flags, "-c", helpers[0], "-o", empty_obj])
        symbols = subprocess.check_output([str(args.xtensa_cc).removesuffix("gcc") + "nm", empty_obj], text=True)
        assert not symbols.strip(), symbols
        receipt["flag_off_source_empty"] = True
    assert receipt["source_sha256"] == {str(path.relative_to(ROOT)): sha(path) for path in tracked}
    (out / "evidence.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print("Two pinned Xtensa links/import/loader gates and exact flag-off Watch comparisons passed; evidence: " + str(out / "evidence.json"))


if __name__ == "__main__":
    main()
