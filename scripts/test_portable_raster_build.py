#!/usr/bin/env python3
"""Explicit bounded-raster selection and disabled-build compatibility."""
import argparse
import hashlib
from pathlib import Path
import tempfile
import unittest
import portable_quick_build as shared

class RasterSelection(unittest.TestCase):
    def arguments(self, selected=False):
        parser=argparse.ArgumentParser()
        shared.options(parser)
        args=parser.parse_args(['--raster-snapshot'] if selected else [])
        args.alarm_client=False
        return args,parser

    def test_disabled_selection_is_unchanged(self):
        args,parser=self.arguments()
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            flags,sources=shared.configure(args,parser,root,root)
            self.assertEqual((flags,sources),([],[]))
            record={};shared.record(args,record);self.assertEqual(record,{})

    def test_enabled_selection_is_explicit_and_source_bound(self):
        args,parser=self.arguments(True)
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            names=('raster_snapshot_state.inc','raster_snapshot_replay.inc')
            for name in names:
                path=root/'lib/PortableApps/src'/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text(name+'\n')
            flags,sources=shared.configure(args,parser,root,root)
            self.assertEqual(flags,['-DPORTABLE_RASTER_SNAPSHOT']);self.assertEqual(sources,[])
            record={};shared.record(args,record);r=record['raster_snapshot']
            self.assertTrue(r['selected']);self.assertFalse(r['model_reentrancy']);self.assertFalse(r['provider_lease_across_model'])
            self.assertTrue(r['direct_frame_compatibility'])
            self.assertEqual(r['allocation_failure'],'complete synchronous fallback')
            self.assertEqual(r['source_sha256'],{str(Path('lib/PortableApps/src')/n):hashlib.sha256((n+'\n').encode()).hexdigest() for n in names})
            ordinary,_=self.arguments();before=[];after=[]
            shared.requirements(ordinary,before);shared.requirements(args,after)
            self.assertEqual(before,after)
            self.assertEqual(shared.configure(args,parser,root,root)[0],flags)

    def test_missing_selected_implementation_is_an_error(self):
        args,parser=self.arguments(True)
        with tempfile.TemporaryDirectory() as directory,self.assertRaises(SystemExit):
            shared.configure(args,parser,Path(directory),Path(directory))

if __name__=='__main__':unittest.main()
