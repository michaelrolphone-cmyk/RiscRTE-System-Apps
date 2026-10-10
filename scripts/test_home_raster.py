#!/usr/bin/env python3
"""Verify pixel-identical compact Home drawing at multiple scales and row slices."""
import os,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='home-raster-') as tmp:
 exe=Path(tmp)/'test'
 subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie',str(root/'test/native_apps/home_raster_test.c'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'),check=True)
