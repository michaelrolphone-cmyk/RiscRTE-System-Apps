import contextlib
import io
from pathlib import Path
import sys
import unittest
from unittest import mock
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
import build_paper_clock as clock

class AtomicQuickSelection(unittest.TestCase):
    def test_reject_incomplete_profile_before_compile(self):
        profiles=[[],['--sparse-start'],['--sparse-start','--resident-shell-host'],
                  ['--sparse-start','--resident-shell-host','--frontlight-tone'],
                  ['--sparse-start','--resident-shell-host','--frontlight-tone','--quick-radios']]
        for profile in profiles:
            with self.subTest(profile=profile),contextlib.redirect_stderr(io.StringIO()),mock.patch.object(clock.subprocess,'run') as run,mock.patch.object(sys,'argv',['build_paper_clock.py','--experimental-atomic-quick',*profile]):
                with self.assertRaises(SystemExit):clock.build()
                run.assert_not_called()

if __name__=='__main__':unittest.main()
