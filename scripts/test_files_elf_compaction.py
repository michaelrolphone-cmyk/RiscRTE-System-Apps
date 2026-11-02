#!/usr/bin/env python3
"""Actual target ELF metadata compaction invariants and corruption rejection."""
import argparse,hashlib,io,json,tempfile,unittest,subprocess
from pathlib import Path
from unittest.mock import patch
from elftools.elf.elffile import ELFFile
from compact_current_elf import compact,evidence
p=argparse.ArgumentParser();p.add_argument('--full',type=Path,required=True);p.add_argument('--compact',type=Path,required=True);p.add_argument('--cc',required=True);a=p.parse_args()
before=a.full.read_bytes();after=a.compact.read_bytes()
def mutated(data):
 elf=ELFFile(io.BytesIO(data));section=next(s for s in elf.iter_sections() if s['sh_flags']&2 and s['sh_type']=='SHT_PROGBITS' and s['sh_size'])
 changed=bytearray(data);changed[section['sh_offset']]^=1;return bytes(changed)
class Compaction(unittest.TestCase):
 def test_shrinks(self):self.assertLess(len(after),len(before))
 def test_all_loader_semantics(self):self.assertEqual(evidence(before),evidence(after))
 def test_allocated_corruption_detected(self):self.assertNotEqual(evidence(before),evidence(mutated(before)))
 def test_entry_corruption_detected(self):
  b=bytearray(before);b[24]^=1;self.assertNotEqual(evidence(before),evidence(bytes(b)))
 def test_repeat_and_full_sidecar(self):
  with tempfile.TemporaryDirectory() as tmp:
   f=Path(tmp)/'app.elf';full=Path(tmp)/'full.elf';f.write_bytes(before)
   compact(f,a.cc,full);self.assertEqual(f.read_bytes(),after);self.assertEqual(full.read_bytes(),before)
 def test_invalid_tool_output_rejected_without_replacement(self):
  with tempfile.TemporaryDirectory() as tmp:
   f=Path(tmp)/'app.elf';full=Path(tmp)/'full.elf';f.write_bytes(before)
   real_run=subprocess.run
   def corrupt(cmd,**kwargs):
    if '--version' in cmd:return real_run(cmd,**kwargs)
    Path(cmd[-1]).write_bytes(mutated(before))
   with patch('compact_current_elf.subprocess.run',side_effect=corrupt):
    with self.assertRaises(ValueError):compact(f,a.cc,full)
   self.assertEqual(f.read_bytes(),before);self.assertFalse(full.exists())
 def test_sidecar_alias_rejected(self):
  with tempfile.TemporaryDirectory() as tmp:
   f=Path(tmp)/'app.elf';f.write_bytes(before)
   with self.assertRaises(ValueError):compact(f,a.cc,f)
   self.assertEqual(f.read_bytes(),before)
if __name__=='__main__':
 print(json.dumps({'full_bytes':len(before),'compact_bytes':len(after),'full_sha256':hashlib.sha256(before).hexdigest(),'compact_sha256':hashlib.sha256(after).hexdigest()}),flush=True)
 unittest.main(argv=['compaction'])
