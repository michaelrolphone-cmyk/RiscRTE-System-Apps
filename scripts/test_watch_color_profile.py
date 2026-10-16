#!/usr/bin/env python3
"""Source and executable admission contract; full product replay is separate."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,sys,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
RUNS=[]
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
  # Compile the production admission statements and native-height layout helpers.
  # The doubles supply only display metadata; they do not reimplement policy.
  source=ROOT/'lib/PortableApps/src/foreground_adapter_open.inc'
  s=source.read_text()
  start=s.index('  if (!display->get_info')
  layout=s.index('#if defined(PORTABLE_RF_NATIVE_CANVAS)',start)
  end=s.index('#ifdef PORTABLE_NATIVE_CUSTODY_FENCE',layout)
  predicate=s[start:end]
  ui=(ROOT/'lib/PortableApps/src/nova_ui.inc').read_text()
  helpers=ui[ui.index('#ifdef PORTABLE_WATCH_NATIVE_HEIGHT'):ui.index('static void nu_pixel')]
  fixture=r'''
#include "RiscDisplayOutputV1.h"
#include <assert.h>
#include <stdio.h>
static risc_display_info_v1 info, offered;
static bool metadata_ok, paper_rotated;
static unsigned surface_format;
static bool metadata(void *context,risc_display_info_v1 *out){(void)context;*out=offered;return metadata_ok;}
static const risc_display_output_api_v1 api={.get_info=metadata};
static const risc_display_output_api_v1 *display=&api;
static bool pp_enabled(void){return true;}
#ifdef PORTABLE_WATCH_NATIVE_HEIGHT
static int width(void){return info.width;}
static int height(void){return info.height;}
''' + helpers + r'''
#endif
static int admitted(unsigned w,unsigned h,unsigned formats,bool readable){
 offered=(risc_display_info_v1){.width=w,.height=h,.supported_formats=formats};
 info=(risc_display_info_v1){0};metadata_ok=readable;
''' + predicate + r'''
 (void)paper_rotated;return 0;
}
int main(void){
 const unsigned rgb=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565);
 const unsigned mono=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_MONO1);
 const unsigned formats[]={rgb,mono,rgb|mono,RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_GRAY2),0};
 /* generic, legacy 240-only, selected native-height expected admission. */
 const struct {unsigned w,h;bool allow[3];} cases[]={
  {159,240,{0,0,0}},{240,159,{0,0,0}},{160,160,{1,0,1}},
  {240,240,{1,1,1}},{240,239,{1,0,0}},{240,241,{1,0,1}},
  {410,502,{1,0,1}},{480,600,{1,0,1}},{502,410,{1,0,0}},
  {410,409,{1,0,1}},{410,408,{1,0,0}},
  {1024,1024,{1,0,1}},{1025,1024,{0,0,0}},{1024,1025,{0,0,0}},
  {160,1024,{1,0,1}},{1024,160,{1,0,0}}};
 unsigned checked=0;
 for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++)
  for(unsigned f=0;f<sizeof(formats)/sizeof(formats[0]);f++)
   for(unsigned readable=0;readable<2;readable++){
    bool expected=cases[i].allow[TEST_PROFILE]&&readable&&
      (TEST_PROFILE?(formats[f]&rgb):!!(formats[f]&(rgb|mono)));
    assert((admitted(cases[i].w,cases[i].h,formats[f],readable)==0)==expected);checked++;
   }
 printf("profile %u: %u actual admission cases PASS\n",TEST_PROFILE,checked);return 0;
}
'''
  with tempfile.TemporaryDirectory(prefix='watch-color-profile-') as directory:
   path=Path(directory);c=path/'admission.c';c.write_text(fixture)
   for profile,defines in enumerate(([],['PORTABLE_WATCH_COLOR_ONLY','PORTABLE_NOVA_UI'],
       ['PORTABLE_WATCH_COLOR_ONLY','PORTABLE_NOVA_UI','PORTABLE_WATCH_NATIVE_HEIGHT'])):
    with self.subTest(profile=profile):
     binary=path/str(profile)
     command=[os.environ.get('CC','cc'),'-std=c11','-O1','-Wall','-Wextra','-Werror',
       '-DPORTABLE_DISPLAY_ROTATION=0',f'-DTEST_PROFILE={profile}',
       *['-D'+flag for flag in defines],'-I'+str(ROOT/'lib/PortableApps/include'),str(c),'-o',str(binary)]
     subprocess.run(command,check=True,capture_output=True,text=True)
     result=subprocess.run([str(binary)],check=True,capture_output=True,text=True)
     RUNS.append({'profile':profile,'defines':defines,'cases':160,'stdout':result.stdout.strip(),
                  'fixture_sha256':hashlib.sha256(fixture.encode()).hexdigest()})
 def test_generic_paper_path_retained(self):
  s=(ROOT/'lib/PortableApps/src/quick_adapter.inc').read_text()
  self.assertIn('#ifndef PORTABLE_WATCH_COLOR_ONLY\n#include "quick_paper.inc"',s)
  self.assertIn('if(quick.ui.paper)quick_paper_render(time,battery_valid,battery.percent);\n    else\n#endif',s)
  self.assertIn('#else\n#include "../fonts/icons.inc"\n#endif',(ROOT/'lib/PortableApps/src/nova.inc').read_text())
if __name__=='__main__':
 parser=argparse.ArgumentParser(add_help=False);parser.add_argument('--evidence',type=Path)
 args,remaining=parser.parse_known_args()
 result=unittest.main(argv=[sys.argv[0],*remaining],exit=False).result
 if args.evidence:
  args.evidence.parent.mkdir(parents=True,exist_ok=True)
  sources=[Path(__file__),ROOT/'lib/PortableApps/src/foreground_adapter_open.inc',
    ROOT/'lib/PortableApps/src/nova_ui.inc',ROOT/'lib/PortableApps/include/WatchLayout.h',
    ROOT/'lib/PortableApps/include/RiscDisplayOutputV1.h']
  args.evidence.write_text(json.dumps({'passed':result.wasSuccessful(),'tests':result.testsRun,
    'normal_admission_cases':sum(r['cases'] for r in RUNS),'runs':RUNS,'hardware':False,
    'source_sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}},indent=2)+'\n')
 sys.exit(0 if result.wasSuccessful() else 1)
