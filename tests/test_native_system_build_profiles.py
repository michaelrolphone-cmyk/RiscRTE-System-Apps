"""Reject ambiguous native ownership before compilation or SDK staging."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]

class NativeSystemBuildProfiles(unittest.TestCase):
    def test_refuses_incomplete_or_conflicting_profile(self):
        native=['--time-profile','x4-native-time','--alarm-client',
                '--native-time-runtime-repo','/unused-runtime','--tagged-alarm-utilities','/unused-utilities']
        for builder in ('springboard','file_browser','wifi'):
            cases=[(['--time-profile','x4-native-time'],'requires --native-time-runtime-repo'),
                   (native+['--wall-time'],'cannot combine'),
                   (native+['--display-rotation','0'],'portrait paper'),
                   (['--native-time-runtime-repo','/unused-runtime'],'require --time-profile'),
                   (['--tagged-alarm-utilities','/unused-utilities'],'require --time-profile'),
                   ([x for x in native if x!='--alarm-client'],'requires --alarm-client')]
            if builder!='file_browser':cases.append((native+['--nova-ui'],'portrait paper'))
            if builder=='springboard':cases.append((native+['--denver'],'cannot combine'))
            if builder=='file_browser':cases.append((native+['--quick-controls'],'cannot combine'))
            for flags,message in cases:
                with self.subTest(builder=builder,flags=flags), tempfile.TemporaryDirectory() as tmp:
                    out=Path(tmp)/'product'
                    result=subprocess.run(['python',str(ROOT/'scripts'/('build_portable_'+builder+'.py')),
                                           *flags,'--output-dir',str(out)],capture_output=True,text=True,
                                          env=dict(os.environ,NATIVE_APP_CC='/must-not-compile'))
                    self.assertEqual(result.returncode,2,result.stderr)
                    self.assertIn(message,result.stderr)
                    self.assertFalse(out.exists())

if __name__=='__main__':unittest.main()
