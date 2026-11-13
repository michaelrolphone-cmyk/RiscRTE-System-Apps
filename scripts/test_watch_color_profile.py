#!/usr/bin/env python3
"""Source contract for opt-in Watch rendering; full product replay is separate."""
from pathlib import Path
import hashlib,json,re,unittest
ROOT=Path(__file__).resolve().parents[1]
class WatchColorProfile(unittest.TestCase):
 def test_frozen_assets_exact(self):
  source=json.loads((ROOT/'lib/PortableApps/fonts/watch-240-icons-source.json').read_text())
  self.assertEqual(source['source_commit'],'14762601b3fa7bbeeac5ed565a6825534bf74dca')
  self.assertEqual(hashlib.sha256((ROOT/'lib/PortableApps/fonts/watch-240-icons.inc').read_bytes()).hexdigest(),source['source_sha256'])
 def test_no_implicit_selection(self):
  for name in ('adapter.c','nova.inc','paper.inc','quick_adapter.inc','foreground_adapter_open.inc'):
   s=(ROOT/'lib/PortableApps/src'/name).read_text()
   self.assertNotRegex(s,r'^\s*#define\s+PORTABLE_WATCH_(COLOR_ONLY|FROZEN_ICONS)\b',name)
 def test_incompatible_panels_rejected(self):
  s=(ROOT/'lib/PortableApps/src/foreground_adapter_open.inc').read_text()
  self.assertIn('#ifdef PORTABLE_WATCH_COLOR_ONLY\n  if(!(info.supported_formats&RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)))return -1;\n#ifndef PORTABLE_WATCH_NATIVE_HEIGHT\n  if(info.width!=240 || info.height!=240)return -1;\n#endif',s)
  self.assertIn('if(surface_format!=RISC_DISPLAY_FORMAT_RGB565 || !nu_layout_ready() || nu_height()<240)return -1;',s)
  self.assertIn('!(info.supported_formats&RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)))return -1;',s)
 def test_generic_paper_path_retained(self):
  s=(ROOT/'lib/PortableApps/src/quick_adapter.inc').read_text()
  self.assertIn('#ifndef PORTABLE_WATCH_COLOR_ONLY\n#include "quick_paper.inc"',s)
  self.assertIn('if(quick.ui.paper)quick_paper_render(time,battery_valid,battery.percent);\n    else\n#endif',s)
  self.assertIn('#else\n#include "../fonts/icons.inc"\n#endif',(ROOT/'lib/PortableApps/src/nova.inc').read_text())
if __name__=='__main__':unittest.main()
