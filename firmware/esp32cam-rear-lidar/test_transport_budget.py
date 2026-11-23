#!/usr/bin/env python3
"""Exercise the production transport with front and rear no-progress budgets.

Packet I/O uses real nonblocking host sockets. A deterministic scheduler clock
makes controlled three-second peer pauses and exact idle deadlines repeatable.
No device or external network is accessed.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    root = Path(__file__).resolve().parent
    shared = root.parent / "esp32cam-rtsp-radio" / "overlay"
    compiler = shutil.which("c++")
    if not compiler:
        raise SystemExit("A host C++ compiler is required")
    patch = root / "live-write-progress.patch"
    extended = root / "overlay/test/live_write_budget"
    with tempfile.TemporaryDirectory(prefix="rear-write-budget-") as directory:
        work = Path(directory)
        (work / "src").mkdir()
        for name in ("live_websocket.h", "live_write_progress.h"):
            shutil.copyfile(shared / "src" / name, work / "src" / name)
        tests = work / "test/live_write_progress"
        shutil.copytree(shared / "test/live_write_progress", tests)
        extended_stubs = work / "extended-stubs"
        shutil.copytree(tests / "stubs", extended_stubs)
        shutil.copyfile(extended / "stubs/esp_timer.h", extended_stubs / "esp_timer.h")
        original = (work / "src/live_write_progress.h").read_text()

        def run(name, source, includes=(), definitions=()):
            binary = work / name
            command = [compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror"]
            for include in includes:
                command.extend(["-I", str(include)])
            command.extend(definitions)
            command.extend([str(source), "-o", str(binary)])
            subprocess.run(command, check=True)
            subprocess.run([str(binary)], check=True)

        for variant, budget in (("front", 1000000), ("rear", 4000000)):
            if variant == "rear":
                subprocess.run(["git", "apply", "--check", str(patch)], cwd=work, check=True)
                subprocess.run(["git", "apply", str(patch)], cwd=work, check=True)
                # The materialized production delta must be the budget alone.
                expected = original.replace("idleLimitUs = 1000000;", "idleLimitUs = 4000000;")
                if original == expected or (work / "src/live_write_progress.h").read_text() != expected:
                    raise AssertionError("The rear patch changed more than the idle budget")
            run(variant + "-progress", tests / "test_progress.cpp")
            run(variant + "-transport", tests / "test_transport.cpp", (tests / "stubs",))
            run(variant + "-recovery", extended / "test_transport.cpp",
                (extended_stubs, work / "src"),
                ("-DEXPECTED_IDLE_LIMIT_US=" + str(budget),))
        if (shared / "src/live_write_progress.h").read_text() != original:
            raise AssertionError("The shared front source was modified")
    print("Front 1s and materialized rear 4s transport gates passed; shared source unchanged")


if __name__ == "__main__":
    main()
