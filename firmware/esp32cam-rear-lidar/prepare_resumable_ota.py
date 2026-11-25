"""Stage the rear-specific ArduinoOTA override from exact ESP32 SDK sources."""
import hashlib
from pathlib import Path
import shutil
import subprocess

FILES = {
    'src/RearArduinoOTA.cpp': ('ArduinoOTA.cpp', '6455975ebfadccbe2acb4dd3a02d714a84b0fece7b32356d63038fc8d5dd9360'),
    'include/RearArduinoOTA.h': ('ArduinoOTA.h', '2dfd8be13d261937d9ec993cfdbd4a3017e3c56cbfc5830301cd50c074e26916'),
}


def apply(project, framework):
    sdk = Path(framework) / "libraries/ArduinoOTA/src"
    for local_name, (original_name, expected) in FILES.items():
        source = sdk / original_name
        if hashlib.sha256(source.read_bytes()).hexdigest() != expected:
            raise ValueError("Unreviewed ArduinoOTA source: " + original_name)
        shutil.copyfile(source, Path(project) / local_name)
    subprocess.run(
        ["git", "apply", "--whitespace=nowarn", str(Path(__file__).with_name("ota-resume-arduino.patch"))],
        cwd=project,
        check=True,
    )
