#!/usr/bin/env python3
import os,pathlib,subprocess
r=pathlib.Path(__file__).resolve().parents[1];out=r/'build/file-browser-paper';out.mkdir(parents=True,exist_ok=True);frames=out/'frames';frames.mkdir(exist_ok=True);binary=out/'file-browser'
subprocess.run([os.environ.get('CC','cc'),'-std=c11','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie','-I'+str(r/'lib/PortableApps/include'),'-I'+str(r/'lib/NativeApps/include'),str(r/'test/native_apps/portable_file_browser_paper_test.c'),'-o',str(binary)],check=True)
subprocess.run([str(binary)],check=True,timeout=30,env={**os.environ,'FILE_BROWSER_PAPER_FRAMES':str(frames)})
