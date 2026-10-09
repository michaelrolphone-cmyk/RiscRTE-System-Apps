#!/usr/bin/env python3
"""Production native-time Settings interaction, custody and raster tests.

Runs the native entry wrapper, unchanged Settings controller and real adapter
with checked native/RTC/KV doubles,
normally and under ASan/UBSan, at 480x800 and 400x600 with optional alarm clients.
Retained provider faults forbid all subsequent I/O and wrapped free calls.
Does not invoke builders or publish artifacts.
Canonical Runtime headers are verified against the pinned Git object and staged
alongside portable headers; the bundled compatibility header is never replaced.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import portable_alarm_build
from datetime import datetime, timezone
from zoneinfo import ZoneInfo

ROOT = Path(__file__).resolve().parents[1]
RUNTIME_REF = "602ae9bd618e13407b5b94bcad86cdabc23c99ea"
CONTROLLER_REF = "c1ff014792c9ecbc195c402e9b4c3e2c3c9d34d3"
HEADERS = ["RiscRuntimeV1.h", "RiscRealtimeV1.h"]
HELPERS = ["PortableSetTime.c", "PortableRealtimeClient.c", "PortableTimeZone.c",
           "PortableTimeZoneCatalog.c", "PortableTimeZonePreference.c"]
CASES = """init-only init-short init-no-retain open-only save-local save-utc save-touch save-touch-flip held-save held-touch
cancel-touch back home value-back value-home value-edit held-entry
fold-first fold-second fold-second-utc fold-held fold-back fold-home gap range
missing-basis bad-basis unavailable-basis missing-zone bad-zone unavailable-zone
basis-context zone-context timezone-io-before timezone-io-after timezone-retry-before timezone-retry-after
unset-local unset-utc native-absent native-control-absent native-context native-read-io
rtc-acquire-false rtc-write-false rtc-read-false rtc-release-false rtc-mismatch
native-seed-io native-readback-io native-readback-mismatch native-seed-context native-retry
metadata-acquire-false metadata-write-io metadata-read-io metadata-release-false metadata-mismatch
metadata-before-io metadata-retry-before metadata-retry-after metadata-read-retry metadata-write-context metadata-read-context metadata-held-save
native-release-false drag-save touch-gap touch-failed-poll touch-replaced timezone timezone-then-save flip-editor
async-edit async-touch async-exit async-retained async-timezone-next sync-timezone-next""".split()
ALARM_CASES = "alarm-step-retained alarm-status-retained alarm-refresh-retained alarm-ack-retained alarm-stop-retained".split()
QUICK_CASES = "quick-startup quick-time quick-time-context quick-later-acquire quick-wifi-disconnect quick-wifi-status quick-ble-set quick-ble-status quick-release-false quick-refresh-retained".split()
PROFILES = {"paper": [], "short-paper": ["-DTEST_NATIVE_SETTINGS_SHORT"],
            "paper-alarms": ["-DTEST_NATIVE_SETTINGS_ALARMS"],
            "short-paper-alarms": ["-DTEST_NATIVE_SETTINGS_SHORT", "-DTEST_NATIVE_SETTINGS_ALARMS"],
            "paper-quick": ["-DTEST_NATIVE_SETTINGS_QUICK"],
            "short-paper-quick": ["-DTEST_NATIVE_SETTINGS_SHORT", "-DTEST_NATIVE_SETTINGS_QUICK"]}
CAPTURE_CASES = {"save-touch", "save-touch-flip", "fold-first", "fold-second", "gap",
                 "missing-basis", "bad-basis", "unavailable-basis", "unset-local",
                 "native-seed-io", "metadata-mismatch", "timezone", "flip-editor",
                 "metadata-write-io", "metadata-read-io", "metadata-before-io", "metadata-retry-before", "metadata-retry-after", "metadata-read-retry",
                 "unavailable-zone", "timezone-io-before", "timezone-io-after", "timezone-retry-before", "timezone-retry-after",
                 "alarm-refresh-retained", "alarm-ack-retained"}


def run(command, **kwargs):
    return subprocess.run(list(map(str, command)), check=True, **kwargs)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def epoch(date):
    return int(datetime.fromisoformat(date).replace(tzinfo=timezone.utc).timestamp())


def environment(name):
    zone = "UTC" if name.endswith("timezone-next") or name.startswith("timezone") or name in {"missing-zone", "bad-zone", "unavailable-zone"} else "America/Denver"
    before = expected = epoch("2026-01-15T19:34:56")
    if name.startswith("fold-"):
        before = expected = epoch("2026-11-01T07:30:00")
        if name in {"fold-second", "fold-second-utc"}:
            expected += 3600
    elif name == "gap":
        before = expected = epoch("2026-03-08T08:30:00")
    elif name == "range":
        before = expected = 2147483647
    elif name == "value-edit":
        expected = epoch("2027-01-15T19:34:56")
    elif name.startswith("unset-"):
        expected = epoch("2000-01-01T07:00:00")
    utc_basis = name in {"save-utc", "fold-second-utc", "unset-utc"}
    save_zone = "America/Denver" if name == "timezone-then-save" else zone
    calendar = expected if utc_basis else int(datetime.fromtimestamp(expected, ZoneInfo(save_zone)).replace(tzinfo=timezone.utc).timestamp())
    return dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1",
                NATIVE_SETTINGS_ZONE=zone, NATIVE_SETTINGS_EPOCH=str(before),
                NATIVE_SETTINGS_EXPECTED_EPOCH=str(expected), NATIVE_SETTINGS_EXPECTED_CALENDAR=str(calendar))


def raster_evidence(frames, output):
    from PIL import Image, ImageChops, ImageDraw
    files = sorted(frames.glob("*.pbm"))
    assert files, "No production framebuffer captures"
    results = {}
    selected = []
    for source in files:
        with Image.open(source) as source_image:
            image = source_image.convert("RGB")
        assert image.size in {(800, 480), (600, 400)}
        assert image.getextrema() != ((255, 255),) * 3, source.name
        destination = output / (source.stem + ".png")
        image.transpose(Image.Transpose.ROTATE_270).save(destination)
        results[destination.name] = {"sha256": sha(destination), "pixel_sha256": hashlib.sha256(image.tobytes()).hexdigest()}
        name, _, suffix = source.stem.partition("-page-")
        del suffix
        # Include each page plus its latest result state in a readable contact sheet.
        case = name.rsplit("-", 1)[0]
        page = source.stem.rsplit("-page-", 1)[1]
        key = (case, page)
        view = image.transpose(Image.Transpose.ROTATE_270)
        view.thumbnail((240, 400))
        existing = next((i for i, item in enumerate(selected) if item[0] == key), None)
        item = (key, source.stem, view)
        if existing is None:
            selected.append(item)
        else:
            selected[existing] = item
    # Independent pixels show that retained flip changes the whole physical
    # display, while identical touch scripts still complete the same Save.
    straight = sorted(frames.glob("save-touch-[0-9]*.pbm"))
    flipped = sorted(frames.glob("save-touch-flip-[0-9]*.pbm"))
    flip_pairs = 0
    if straight and flipped:
        assert len(straight) == len(flipped)
        for first, second in zip(straight, flipped):
            with Image.open(first) as a, Image.open(second) as b:
                expected = a.convert("RGB").transpose(Image.Transpose.ROTATE_180)
                assert ImageChops.difference(expected, b.convert("RGB")).getbbox() is None, (first.name, second.name)
            flip_pairs += 1
    columns, width, height = 4, 260, 438
    sheet = Image.new("RGB", (columns * width, ((len(selected) + columns - 1) // columns) * height), "#dddddd")
    draw = ImageDraw.Draw(sheet)
    for i, (_, title, image) in enumerate(selected):
        x, y = (i % columns) * width, (i // columns) * height
        draw.text((x + 5, y + 5), title, fill="black")
        sheet.paste(image, (x + 10, y + 30))
    sheet_path = output / "contact-sheet.png"
    sheet.save(sheet_path)
    return {"captures": results, "verified_180_degree_pairs": flip_pairs,
            "contact_sheet": str(sheet_path), "contact_sheet_sha256": sha(sheet_path)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime-sdk", type=Path, default=ROOT.parent / "x4-runtime-provider-retention/sdk/app")
    parser.add_argument("--runtime-ref", default=RUNTIME_REF)
    parser.add_argument("--output-dir", type=Path, default=ROOT / "build/native-time-settings")
    parser.add_argument("--evidence", type=Path)
    parser.add_argument("--case", action="append", choices=CASES + ALARM_CASES + QUICK_CASES, dest="cases")
    parser.add_argument("--profile", action="append", choices=PROFILES, dest="profiles")
    parser.add_argument("--normal-only", action="store_true", help="Development diagnostic run; final verification uses both builds")
    parser.add_argument("--no-pixels", action="store_true", help="Skip optional production raster export")
    portable_alarm_build.options(parser)
    args = parser.parse_args()
    args.alarm_client=True
    sdk = args.runtime_sdk.resolve()
    repo = sdk.parents[1]
    assert args.runtime_ref == RUNTIME_REF, "This fixture qualifies the exact canonical Runtime pin"
    for header in HEADERS:
        canonical = subprocess.check_output(["git", "-C", repo, "show", args.runtime_ref + ":sdk/app/" + header])
        assert canonical == (sdk / header).read_bytes(), header + " is not the pinned canonical header"
    controller = ROOT / "Apps/settings.c"
    assert controller.read_bytes() == subprocess.check_output(["git", "-C", ROOT, "show", CONTROLLER_REF + ":Apps/settings.c"]), "Portable Settings controller changed"
    bundled = ROOT / "lib/PortableApps/include/RiscRuntimeV1.h"
    old_header = bundled.read_bytes()
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    cc = os.environ.get("CC", "cc")
    sources = [ROOT / "Apps/settings_native_entry.c", ROOT / "test/native_apps/portable_native_time_settings_test.c",
               *[ROOT / "lib/PortableApps/src" / name for name in HELPERS]]
    receipt = {"purpose": "production Settings controller/adapter/helper host composition; native providers are test doubles",
               "version": "1.3.7", "runtime_ref": RUNTIME_REF,
               "portable_controller_ref": CONTROLLER_REF, "portable_controller_unchanged": True,
               "native_entry": "Apps/settings_native_entry.c",
               "runtime_tree": subprocess.check_output(["git", "-C", repo, "rev-parse", RUNTIME_REF + "^{tree}"], text=True).strip(),
               "runtime_headers": {name: sha(sdk / name) for name in HEADERS},
               "compiler": subprocess.check_output([cc, "--version"], text=True).splitlines()[0],
               "target_builder_run": False, "hardware_qualification": "not run",
               "timezone_next_coverage": "Real Settings touch/controller/renderer; fixed simulated 1000 ms transfers; times are relative to first region submission. Async and synchronous present-status paths receive the same relative tap timeline. Detection is first neutral snapshot, dispatch is observed page state change, visibility is provider completion of that exact page; coalesced intermediate pages have null visibility. Hardware timing is not measured.",
               "quick_coverage": "startup composition and direct production clock/action calls before fini; Quick gestures not qualified here",
               "runs": {}}
    with tempfile.TemporaryDirectory(prefix="native-time-settings-") as temporary:
        stage = Path(temporary)
        include = stage / "include"
        shutil.copytree(ROOT / "lib/PortableApps/include", include)
        shutil.copytree(ROOT / "lib/PortableApps/time", stage / "time")
        for header in HEADERS:
            shutil.copyfile(sdk / header, include / header)
        tagged=portable_alarm_build.stage(args,parser,out,include)
        if tagged:receipt['tagged_alarm_sdk']=tagged
        for profile in args.profiles or PROFILES:
            quick = "quick" in profile
            alarms = "alarms" in profile or quick
            selected_cases = args.cases or CASES + (ALARM_CASES if alarms else []) + (QUICK_CASES if quick else [])
            selected_cases = [case for case in selected_cases if (case not in ALARM_CASES or alarms) and (case not in QUICK_CASES or quick)]
            if not selected_cases:
                continue
            profile_sources = [*sources, *([ROOT / "lib/PortableApps/src" / name for name in
                                ("quick_actions.c", "quick_render.c", "quick_session.c", "quick_radios.c")] if quick else [])]
            profile_out = out / profile
            profile_out.mkdir(exist_ok=True)
            frames = profile_out / "frames"
            for sanitized in ([False] if args.normal_only else [False, True]):
                label = profile + ("-asan-ubsan" if sanitized else "-normal")
                executable = out / ("native-time-settings-" + label)
                flags = ["-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer", "-no-pie"] if sanitized else []
                run([cc, "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-DPORTABLE_NATIVE_CUSTODY_FENCE", *flags, *PROFILES[profile],
                     *(["-DALARM_SERVICE_TAGGED_V2"] if tagged else []),
                     "-I" + str(include), "-I" + str(ROOT / "lib/NativeApps/include"), *profile_sources,
                     "-Wl,--wrap=free", "-o", executable], timeout=120)
                results = []
                if not sanitized and not args.no_pixels:
                    frames.mkdir(exist_ok=True)
                    for old in frames.glob("*.pbm"):
                        old.unlink()
                with (out / (label + ".log")).open("w") as log:
                    for name in selected_cases:
                        env = environment(name)
                        if not sanitized and not args.no_pixels and name in CAPTURE_CASES:
                            env["NATIVE_SETTINGS_FRAMES"] = str(frames)
                        result = run([executable, name], env=env, text=True, stdout=subprocess.PIPE, timeout=20)
                        log.write(result.stdout)
                        results.append(json.loads(result.stdout.strip().splitlines()[-1]))
                receipt["runs"][label] = results
                print(f"Native-time Settings {label}: {len(results)} production interaction cases passed", flush=True)
            if not args.no_pixels and any(case in CAPTURE_CASES for case in selected_cases):
                receipt.setdefault("raster", {})[profile] = raster_evidence(frames, profile_out)
    tracked = [*sources, ROOT / "Apps/settings.c", ROOT / "Apps/PaperFrame.h", ROOT / "lib/PortableApps/src/adapter.c", ROOT / "lib/PortableApps/src/settings.inc",
               ROOT / "lib/PortableApps/src/settings_native_time.inc", ROOT / "lib/PortableApps/src/settings_view.inc",
               ROOT / "lib/PortableApps/src/settings_paper.inc", ROOT / "lib/PortableApps/src/settings_timezone.inc",
               ROOT / "lib/PortableApps/src/settings_timezone_view.inc", ROOT / "lib/PortableApps/src/native_custody_adapter.inc",
               ROOT / "lib/PortableApps/src/alarm.inc", ROOT / "lib/PortableApps/include/PortableTouch.h",
               ROOT / "lib/PortableApps/include/PortableAlarmClient.h", ROOT / "lib/PortableApps/include/PortableNativeCustody.h",
               *[ROOT / "lib/PortableApps/src" / name for name in
                 ("quick_actions.c", "quick_render.c", "quick_session.c", "quick_radios.c", "quick_adapter.inc")],
               Path(__file__).resolve()]
    receipt["source_sha256"] = {str(path.relative_to(ROOT)): sha(path) for path in tracked if path.is_file()}
    assert bundled.read_bytes() == old_header, "Bundled compatibility Runtime header changed"
    receipt["bundled_runtime_header_unchanged"] = True
    receipt["process_cases"] = sum(map(len, receipt["runs"].values()))
    destination = args.evidence or out / "evidence.json"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(receipt, indent=2) + "\n")
    print(f"{receipt['process_cases']} fresh-process cases passed; evidence: {destination}")


if __name__ == "__main__":
    main()
