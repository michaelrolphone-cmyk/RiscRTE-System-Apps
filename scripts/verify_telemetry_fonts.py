#!/usr/bin/env python3
"""Regenerate every glyph in scratch while retaining qualified shipped rasters."""
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FONT = ROOT / 'Services/scene_host/fonts'


def main():
    qualified = (FONT / 'telemetry.inc').read_bytes()
    assert hashlib.sha256(qualified).hexdigest() == '928e6c01d863767a2fbb641963a8c9f63e76243c95d52208caaa62961909f7a6'
    with tempfile.TemporaryDirectory(prefix='telemetry-font-verification-') as tmp:
        generated, metadata = Path(tmp) / 'telemetry.inc', Path(tmp) / 'sources.json'
        subprocess.run([sys.executable, str(ROOT / 'scripts/generate_telemetry_fonts.py'),
                        '--output', str(generated), '--metadata-output', str(metadata)], check=True)
        data = generated.read_bytes()
        assert hashlib.sha256(data).hexdigest() == '2d7501e42d6223ebbc191817715431182c435fea944a1a02166a0748a577eaf1'
        # The sole permitted difference is the explicitly distinct provenance
        # comment. Every glyph declaration/table and source record stays exact.
        assert qualified.split(b'\n', 1)[1] == data.split(b'\n', 1)[1]
        assert metadata.read_bytes() == (FONT / 'TELEMETRY-SOURCES.json').read_bytes()
    assert qualified == (FONT / 'telemetry.inc').read_bytes()
    print('PASS qualified/generated hashes, every glyph byte and source record; shipped fonts unchanged')


if __name__ == '__main__':
    main()
