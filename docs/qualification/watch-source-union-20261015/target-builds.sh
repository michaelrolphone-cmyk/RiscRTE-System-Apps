set -euo pipefail
export NATIVE_APP_CC=/workspace/scratch/c744abbbbd60/watch-build-tools/platformio-core/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc
export PYTHONDONTWRITEBYTECODE=1
export ASAN_OPTIONS=detect_leaks=0
export PLATFORMIO_CORE_DIR=/workspace/scratch/c744abbbbd60/watch-build-tools/platformio-core
/workspace/scratch/c744abbbbd60/watch-build-tools/python/bin/python scripts/build_all_apps.py
/workspace/scratch/c744abbbbd60/watch-build-tools/python/bin/python scripts/check_release_parity.py
/workspace/scratch/c744abbbbd60/watch-build-tools/python/bin/python scripts/build_portable_settings.py
/workspace/scratch/c744abbbbd60/watch-build-tools/python/bin/python scripts/build_portable_settings.py --display-rotation 90 --nova-ui --alarm-client --alarm-settings --navigation --output-dir dist/portable/settings-paper
/workspace/scratch/c744abbbbd60/watch-build-tools/python/bin/python scripts/build_portable_settings.py --nova-ui --alarm-client --alarm-settings --sleep-settings --denver --navigation --full-frames --output-dir dist/portable/settings-nova
/workspace/scratch/c744abbbbd60/watch-build-tools/python/bin/python scripts/build_portable_updates.py --services-only --rtc-utc-offset-seconds 28800
/workspace/scratch/c744abbbbd60/watch-build-tools/python/bin/python scripts/build_portable_file_browser.py --alarm-client --navigation
/workspace/scratch/c744abbbbd60/watch-build-tools/python/bin/python scripts/build_portable_file_browser.py --display-rotation 90 --storage-capability storage.volume --return-app springboard.elf --output-dir dist/portable/file-browser-paper
/workspace/scratch/c744abbbbd60/watch-build-tools/python/bin/python scripts/build_portable_file_browser.py --display-rotation 90 --storage-capability storage.volume --storage-instance 11 --secondary-storage-instance 22 --file-handlers --return-app springboard.elf --output-dir dist/portable/file-browser-contract-test
/workspace/scratch/c744abbbbd60/watch-build-tools/python/bin/python scripts/build_portable_file_browser.py --alarm-client --navigation --quick-controls --output-dir dist/portable/file-browser-quick
/workspace/scratch/c744abbbbd60/watch-build-tools/python/bin/python scripts/build_portable_wifi.py --alarm-client --navigation --full-frames --wifi-instance 15
/workspace/scratch/c744abbbbd60/watch-build-tools/python/bin/python scripts/build_portable_wifi.py --nova-ui --alarm-client --navigation --full-frames --wifi-instance 15 --output-dir dist/portable/wifi-nova
/workspace/scratch/c744abbbbd60/watch-build-tools/python/bin/python scripts/build_portable_settings.py --alarm-client --alarm-settings --sleep-settings --full-frames --output-dir dist/portable/settings-alarm
/workspace/scratch/c744abbbbd60/watch-build-tools/python/bin/python scripts/build_portable_springboard.py --alarm-client --full-frames --output-dir dist/portable/springboard-alarm
/workspace/scratch/c744abbbbd60/watch-build-tools/python/bin/python scripts/build_portable_springboard.py --nova-ui --alarm-client --denver --full-frames --retained-rgb565-handoff --handoff-ms 60 --return-app clock.elf --output-dir dist/portable/springboard-nova
/workspace/scratch/c744abbbbd60/watch-build-tools/python/bin/python scripts/build_portable_settings.py --denver --full-frames --sleep-settings --output-dir dist/portable/settings-sleep
/workspace/scratch/c744abbbbd60/watch-build-tools/python/bin/python scripts/build_portable_springboard.py --denver --full-frames --retained-rgb565-handoff --handoff-ms 60 --return-app clock.elf --output-dir dist/portable/springboard-handoff
/workspace/scratch/c744abbbbd60/watch-build-tools/python/bin/python scripts/build_portable_springboard.py
/workspace/scratch/c744abbbbd60/watch-build-tools/python/bin/python scripts/build_portable_springboard.py --navigation --display-rotation 90 --output-dir dist/portable/springboard-paper
/workspace/scratch/c744abbbbd60/watch-build-tools/python/bin/python scripts/build_portable_springboard.py --denver --touch-rotation 180 --output-dir dist/portable/springboard-denver
/workspace/scratch/c744abbbbd60/watch-build-tools/python/bin/python scripts/build_portable_settings.py --denver --touch-rotation 180 --output-dir dist/portable-denver
/workspace/scratch/c744abbbbd60/watch-build-tools/python/bin/python scripts/build_portable_settings.py --denver --navigation --touch-rotation 180 --output-dir dist/portable-navigation
