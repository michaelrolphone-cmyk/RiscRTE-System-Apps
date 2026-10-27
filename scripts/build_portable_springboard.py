#!/usr/bin/env python3
"""Build the shared Springboard app against the minimal capability client.

Development artifact only. The deployment supplies display.output@1,
input.touch.raw@1 grants; RTC requires an explicitly selected deployment policy; no additional runtime exports.
"""
from native_toolchain import compiler_path as resolve_compiler
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import portable_broadcast_build
import portable_quick_build
import portable_idle_build
import portable_foreground_idle_build
import portable_native_toolbar_build
import portable_performance_build
import portable_native_performance_build
import portable_raster_memory_build
import portable_paper_build
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def catalog_source(path,limit=18,authority_boot=None):
    if path is None:
        return '#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n',None
    raw=path.read_bytes()
    if len(raw)>32768:raise ValueError('Catalog exceeds 32 KiB')
    data=json.loads(raw)
    if not isinstance(data,dict) or set(data)!={'apps'} or not isinstance(data['apps'],list) or len(data['apps'])>limit:raise ValueError('Catalog must contain at most '+str(limit)+' apps')
    excluded=[];styles=[]
    if authority_boot:
        boot=json.loads(authority_boot.read_text())
        for policy in boot['app_capabilities']:
            if 'image_source' not in policy:continue
            manifest=json.loads((authority_boot.parent/policy['manifest']).read_text())
            excluded.append(manifest['file_name'])
            old=next((a for a in data['apps'] if a.get('file_name')==manifest['file_name']),None)
            if old:
                style=dict(old);style['file_name']=str(Path(policy['image_source']['path']).parent)+'/'
                if not re.fullmatch(r'Apps/[A-Za-z0-9_.-]+/',style['file_name']) or len(style['file_name'])>=128:raise ValueError('Invalid legacy style directory')
                styles.append(style)
        data['apps']=[a for a in data['apps'] if a.get('file_name') not in excluded]
    font_sources=json.loads((ROOT/'lib/PortableApps/fonts/SOURCES.json').read_text())
    glyphs=list(font_sources['icons'])
    if limit==40:glyphs+=list(font_sources.get('selected_springboard_icons',{}))
    seen=set();rows=[]
    for app in data['apps']:
        if not isinstance(app,dict) or set(app)!={'display_name','file_name','icon'}:raise ValueError('Catalog entry requires display_name,file_name,icon')
        for name,bound in [('display_name',96),('file_name',128),('icon',24)]:
            value=app[name]
            if not isinstance(value,str) or not value or len(value.encode())>=bound or any(ord(c)<32 or ord(c)>126 for c in value):raise ValueError('Invalid bounded catalog '+name)
        filename=app['file_name']
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*\.elf',filename) or filename.lower() in seen:raise ValueError('Invalid or duplicate catalog filename')
        if app['icon'] not in glyphs:raise ValueError('Catalog icon is absent from licensed subset')
        seen.add(filename.lower())
        rows.append('{'+','.join('.'+key+'='+json.dumps(app[key]) for key in ['display_name','file_name','icon'])+',.compatible=true}')
    source='#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[]={'+(','.join(rows) if rows else '{.compatible=false}')+'};\nconst unsigned portable_catalog_count='+str(len(rows))+';\n'
    if authority_boot:
        source+='const t5_app_manifest_t portable_catalog_legacy_styles[]={'+(','.join('{'+','.join('.'+key+'='+json.dumps(a[key]) for key in ['display_name','file_name','icon'])+'}' for a in styles) if styles else '{0}')+'};\nconst unsigned portable_catalog_legacy_style_count='+str(len(styles))+';\n'
    return source,{'sha256':hashlib.sha256(raw).hexdigest(),'count':len(rows),'apps':data['apps'],'sd_authority_files':excluded,'authority_boot_sha256':hashlib.sha256(authority_boot.read_bytes()).hexdigest() if authority_boot else None}

def build():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--springboard-gesture-diagnostics',action='store_true',help='Explicit bounded Springboard-only decoded/mapped/delta terminal gesture summaries; no keyboard/modal coordinates')
    parser.add_argument("--last-attempt-checkpoint",action="store_true",help="Opt-in bounded RAM checkpoint before callbacks; names attempts, not failure causes")
    parser.add_argument("--safe-viewport",action="store_true",help="Use capability safe insets without changing the physical raster")
    parser.add_argument("--touch-scrolling",action="store_true",help="Explicit native X4 horizontal paged paper app grid")
    parser.add_argument("--installed-catalog-volume-instance",type=int,help="Opt in to cached installed-app discovery on this async storage volume")
    parser.add_argument("--installed-authority-boot",type=Path,help="Exclude removable bootstrap identities from the compiled installed list; preserve them only as SD authority reservations")
    parser.add_argument("--catalog",type=Path,help="Explicit bounded installed/admitted deployment catalog JSON")
    parser.add_argument("--display-rotation",type=int,choices=[0,90],default=None,help="Software portrait mapping for a native retaining MONO1 surface; raw touch is already logical")
    parser.add_argument("--navigation",action="store_true",help="Bind generic input.navigation alongside raw touch")
    parser.add_argument("--watch-native-ui",action="store_true",help="Explicit shared Watch color presentation at capability-native dimensions; unknown refresh is not a performance claim")
    portable_raster_memory_build.add_arguments(parser)
    parser.add_argument("--nova-mask-bounds",action="store_true",help="Prove untouched black mask regions from immutable native commands (default off)")
    parser.add_argument("--sliced-native-frames",action="store_true",help="Explicit immutable native Nova replay with completed-frame hit maps")
    parser.add_argument("--nova-ui",action="store_true",help="Settings-derived 240x240 shared utility profile")
    parser.add_argument("--alarm-client",action="store_true",help="Explicit alarm.service foreground overlay consumer")
    parser.add_argument("--wall-time",action="store_true",help="Read RTC wall time without timezone conversion")
    parser.add_argument("--denver",action="store_true",help="Select RTC UTC+08 to America/Denver display policy")
    parser.add_argument("--rotation","--touch-rotation",dest="rotation",type=int,choices=[0,180],default=0)
    parser.add_argument("--output-dir",type=Path,default=ROOT/"dist/portable")
    parser.add_argument("--retained-rgb565-handoff",action="store_true",help="Opt in only when the deployment guarantees a completed retained frame; see PORTABLE_TRANSITIONS.md")
    parser.add_argument("--full-frames",action="store_true",help="Disable optional partial-damage and previous-frame cache")
    parser.add_argument("--handoff-ms", type=int, choices=[60,180], default=180)
    parser.add_argument("--return-app", help="Explicit root-Back destination .elf")
    portable_broadcast_build.options(parser)
    portable_quick_build.options(parser)
    portable_foreground_idle_build.options(parser)
    portable_native_toolbar_build.options(parser)
    portable_performance_build.options(parser)
    portable_native_performance_build.options(parser)
    portable_paper_build.options(parser)
    args=parser.parse_args()
    portable_native_performance_build.validate(args,parser,args.time_profile)
    if args.springboard_gesture_diagnostics and (not args.native_performance_trace or not args.watch_native_ui):parser.error('Springboard gesture diagnostics require explicit native Watch RAM trace')
    portable_foreground_idle_build.validate(args,parser)
    portable_native_toolbar_build.validate(args,parser)
    portable_broadcast_build.validate(args,parser,portable_native_toolbar_build.selected(args))
    portable_paper_build.validate(args,parser)
    if args.touch_scrolling and (not portable_native_toolbar_build.selected(args) or (not args.paper_transitions and not args.resident_shell_client)):
        parser.error("--touch-scrolling requires --time-profile x4-native-time and --paper-transitions")
    if args.installed_authority_boot and args.installed_catalog_volume_instance is None:parser.error("Authority migration requires installed catalog")
    if args.installed_catalog_volume_instance is not None:
        if args.installed_catalog_volume_instance<1 or not args.touch_scrolling or not args.resident_shell_client:
            parser.error('Installed catalog requires a positive volume instance and resident touch-scrolling Springboard')
    if args.watch_native_ui and (args.safe_viewport or args.touch_scrolling):parser.error("Watch native presentation uses its own color geometry, not paper/safe-viewport profiles")
    if args.sliced_native_frames and (not args.watch_native_ui or args.time_profile!='watch-native-time'):
        parser.error("Sliced native frames require the current Watch native-time presentation")
    if args.nova_mask_bounds and not args.sliced_native_frames:parser.error("Nova mask bounds require --sliced-native-frames")
    if args.watch_native_ui:args.nova_ui=True
    if args.wall_time and args.denver:parser.error("Choose one explicit RTC policy")
    flags=["-DPORTABLE_TOUCH_ROTATION="+str(args.rotation)]+(["-DPORTABLE_RTC_UTC8_DENVER"] if args.denver else [])
    if args.touch_scrolling:flags += ["-DPORTABLE_TOUCH_SCROLL","-DPORTABLE_APP_TOUCH_SCROLL","-DPORTABLE_SPRINGBOARD_TOUCH_SCROLL","-DPORTABLE_NOVA_UI"]
    if args.wall_time:flags.append("-DPORTABLE_RTC_WALL_TIME")
    if args.handoff_ms!=180: flags.append("-DPORTABLE_HANDOFF_EAGER_MS="+str(args.handoff_ms))
    if args.return_app: flags.append('-DPORTABLE_RETURN_APP="'+args.return_app+'"')
    flags.append("-DPORTABLE_DISPLAY_ROTATION="+str(args.display_rotation))
    if args.safe_viewport: flags.append('-DPORTABLE_SAFE_VIEWPORT')
    if args.navigation: flags.append("-DPORTABLE_INPUT_NAVIGATION")
    if args.nova_ui: flags.append("-DPORTABLE_NOVA_UI")
    if args.watch_native_ui:flags += ["-DPORTABLE_WATCH_NATIVE_HEIGHT","-DPORTABLE_WATCH_COLOR_ONLY"]
    if args.springboard_gesture_diagnostics:flags.append('-DPORTABLE_SPRINGBOARD_GESTURE_TRACE')
    if args.sliced_native_frames:flags += ["-DPORTABLE_NOVA_SLICED_FRAMES","-DPORTABLE_RASTER_WORK_BUDGET_MS=8","-DPORTABLE_FRAME_RECEIPTS","-DPORTABLE_RASTER_SNAPSHOT","-DPORTABLE_INPUT_SAMPLE_INTERVAL_MS=2u","-DPORTABLE_RASTER_COOPERATIVE_INPUT","-DPORTABLE_RGB565_SPAN_FILL"]
    if args.alarm_client: flags.append("-DPORTABLE_ALARM_CLIENT")
    if args.full_frames: flags.append("-DPORTABLE_FORCE_FULL_FRAMES")
    if args.retained_rgb565_handoff: flags.append("-DPORTABLE_RETAINED_RGB565_HANDOFF")
    cc = resolve_compiler()
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    notices=out/'licenses/portable-apps';notices.mkdir(parents=True,exist_ok=True)
    for name in ['LICENSE-FontAwesome.txt','LICENSE-Orbitron.txt','LICENSE-Rajdhani.txt','SOURCES.json']:
        shutil.copyfile(ROOT/'lib/PortableApps/fonts'/name,notices/name)
    paper_notices=notices/"paper";paper_notices.mkdir(exist_ok=True)
    for name in ["LICENSE-Orbitron.txt","LICENSE-Rajdhani.txt","SOURCES.json"]:
        shutil.copyfile(ROOT/"lib/PortableApps/paper_fonts"/name,paper_notices/name)
    includes,native_flags,native_sources,native_receipt=portable_native_toolbar_build.configure(args,parser,ROOT,out,'springboard');flags+=native_flags
    includes,paper_flags,paper_transition=portable_paper_build.stage(args,parser,ROOT,out,includes);flags+=paper_flags
    if args.installed_catalog_volume_instance is not None:
        flags += ['-DPORTABLE_INSTALLED_CATALOG','-DPORTABLE_INSTALLED_VOLUME_INSTANCE='+str(args.installed_catalog_volume_instance)+'ULL']
        shutil.copyfile(args.native_time_runtime_repo/'sdk/app/RiscStorageVolumeV1.h',includes/'RiscStorageVolumeV1.h')
    quick_flags,quick_sources=portable_quick_build.configure(args,parser,ROOT,out,includes);flags+=quick_flags
    flags+=portable_broadcast_build.flags(args)
    includes,idle_flags,idle_sources=portable_foreground_idle_build.configure(args,parser,ROOT,out,includes)
    flags+=idle_flags
    flags += portable_raster_memory_build.configure(args,parser,flags)
    if args.nova_mask_bounds:flags.append("-DPORTABLE_NOVA_MASK_BOUNDS")
    quick_sources+=idle_sources
    native_perf_flags,native_perf_receipt=portable_native_performance_build.configure(args,parser,includes)
    flags+=native_perf_flags
    if args.last_attempt_checkpoint:
        if "-DPORTABLE_NATIVE_CUSTODY_FENCE" not in flags:parser.error("Last-attempt checkpoint requires native custody fence")
        flags.append("-DPORTABLE_LAST_ATTEMPT_CHECKPOINT")
        for name in ["PortableLastAttempt.h", "RiscDiagnosticCheckpointV1.h"]:
            shutil.copyfile(ROOT/"lib/PortableApps/include"/name,includes/name)
    exports=portable_quick_build.exports(args,{'app_main', 'app_module_init', 'app_module_fini'})
    mapping = out/'springboard.map'
    mapping.write_text('{ global: '+ '; '.join(sorted(exports))+'; local: *; };\n')
    catalog = out/'springboard-catalog.c'
    catalog_text,catalog_record=catalog_source(args.catalog,40 if args.touch_scrolling else 18,args.installed_authority_boot)
    catalog_bound=40 if args.installed_catalog_volume_instance is not None else max(1,catalog_record['count'] if catalog_record else 0)
    if args.touch_scrolling:flags.append('-DPORTABLE_SPRINGBOARD_CATALOG_BOUND='+str(catalog_bound))
    catalog.write_text(catalog_text)
    if args.installed_authority_boot:flags.append('-DPORTABLE_INSTALLED_AUTHORITY_STYLES')
    elf = out/'springboard.elf'
    sources = [ROOT/'Apps/springboard.c', ROOT/'lib/PortableApps/src/adapter.c', ROOT/'lib/NativeApps/src/SingleFloatDivisionCompat.c', catalog]+quick_sources+native_sources
    if args.installed_catalog_volume_instance is not None:sources.append(ROOT/'lib/PortableApps/src/PortableInstalledCatalog.c')
    compile_command=[cc, '-std=c11', '-Os', '-fPIC', '-mtext-section-literals', '-mlongcalls',
        '-fvisibility=hidden', '-ffreestanding', '-fno-builtin', '-nostdlib', '-nostartfiles', '-shared',
        '-Wl,--hash-style=sysv', '-Wl,--version-script='+str(mapping), '-Wall', '-Wextra', '-Werror', *flags,
        '-I'+str(includes),
        '-I'+str(ROOT/'lib/NativeApps/include'), *map(str, sources), '-o', str(elf)]
    subprocess.run(compile_command, check=True, timeout=120)
    symbols = subprocess.check_output([cc.removesuffix('gcc')+'nm', '-D', str(elf)], text=True)
    imports = {line.split()[-1] for line in symbols.splitlines() if ' U ' in ' '+line}
    allowed = {'risc_runtime_get_api','memcpy','memset','memcmp','memchr','strcmp','strlen','snprintf','strcpy','malloc','free'}
    if args.installed_authority_boot:allowed.add('strncmp')
    if not imports <= allowed: raise ValueError('Unexpected imports: '+str(imports-allowed))
    actual = {line.split()[-1] for line in symbols.splitlines() if len(line.split())>=3 and line.split()[-2] in ('T','D','B','R')}
    if actual != exports: raise ValueError('Unexpected exports: '+str(actual))
    data = elf.read_bytes()
    if data[:7] != b'\x7fELF\x01\x01\x01' or data[16:20] != b'\x03\x00\x5e\x00':
        raise ValueError('Expected ELF32 little-endian Xtensa ET_DYN')
    validator = ROOT/'build/portable-validate-elf'
    validator.parent.mkdir(exist_ok=True)
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
        '-I'+str(ROOT/'test/native_apps/stubs'),'-I'+str(ROOT/'lib/elf_loader/include'),
        str(ROOT/'lib/elf_loader/src/esp_elf_validate.c'),str(ROOT/'test/native_apps/validate_test.c'),
        '-o',str(validator)],check=True,timeout=60)
    subprocess.run([str(validator),str(elf)],check=True,timeout=60)
    version=native_receipt['version'] if native_receipt else json.loads((ROOT/'Apps/springboard.json').read_text())['version']
    if paper_transition or args.paper_transitions:
        version=portable_paper_build.VERSIONS['springboard']
        if native_receipt:native_receipt['version']=version
    version=portable_broadcast_build.version(args,'springboard',version)
    version=portable_idle_build.version(args,'springboard',version)
    version=portable_paper_build.touch_scroll_version(args,'springboard',version)
    version=portable_quick_build.version(args,'springboard',version)
    if native_receipt:native_receipt['version']=version
    manifest={'type':'application','id':'springboard','version':version,'architecture':'xtensa-esp32s3',
        'file_name':'springboard.elf','entry':'app_main','requires':[
            {'capability':'display.output','api':1}, {'capability':'input.touch.raw','api':1}, {'capability':'board.battery','api':1},
]}
    if args.installed_catalog_volume_instance is not None:
        manifest['requires'] += [{'capability':'software.installed-apps','api':1},{'capability':'storage.volume','api':1}]
    if args.navigation: manifest["requires"].append({"capability":"input.navigation","api":1})
    if args.alarm_client: manifest["requires"].append({"capability":"alarm.service","api":1})
    if args.denver or args.wall_time: manifest['requires'].append({'capability':'rtc.clock','api':2})
    portable_quick_build.requirements(args,manifest['requires'])
    portable_native_toolbar_build.requirements(args,manifest['requires'])
    portable_broadcast_build.requirements(args,manifest['requires'])
    portable_foreground_idle_build.requirements(args,manifest['requires'])
    (out/'springboard.json').write_text(json.dumps(manifest,indent=2)+'\n')
    inputs=['Apps/SpringboardPaperHeader.h','scripts/generate_springboard_header_fonts.py','Apps/springboard_pages.h','Apps/springboard_scroll.inc','Apps/PaperBattery.h','Apps/PaperFrame.h','scripts/portable_quick_build.py','Apps/PaperPresentation.h','Apps/springboard_paper.inc','lib/PortableApps/include/PortableTransition.h','Apps/springboard.c','Apps/springboard.json','lib/PortableApps/src/adapter.c',
            'lib/PortableApps/src/nova.inc','Apps/springboard_nova.inc','Apps/springboard_motion.h','Apps/SpringboardPresentation.h','lib/PortableApps/fonts/icons.inc','lib/PortableApps/fonts/text.inc','lib/PortableApps/fonts/SOURCES.json','lib/NativeApps/src/SingleFloatDivisionCompat.c','lib/PortableApps/include/PortableRtcClock.h',
            'lib/PortableApps/RTC_PROVENANCE.json','lib/PortableApps/SOURCES.json']
    inputs += ['lib/PortableApps/include/'+name for name in json.loads((ROOT/'lib/PortableApps/SOURCES.json').read_text())]
    inputs += ['lib/PortableApps/include/PortableTime.h','lib/PortableApps/time/denver/display_time.h','lib/PortableApps/time/denver/twatch_calendar.h','lib/PortableApps/time/denver/twatch_caps.h','lib/PortableApps/time/SOURCES.json']
    inputs += ['lib/PortableApps/include/PortableApps.h','lib/PortableApps/include/PortableTouch.h',
               'lib/NativeApps/include/T5AppApi.h','lib/NativeApps/include/T5UiApi.h',
               'lib/NativeApps/include/T5StorageApi.h','lib/NativeApps/include/T5VideoApi.h']
    inputs += [str(p.relative_to(ROOT)) for p in sorted((ROOT/'lib/PortableApps').rglob('*')) if p.is_file()]
    inputs=sorted(set(inputs))
    inputs.append('scripts/portable_raster_memory_build.py')
    record={'nova_mask_bounds':args.nova_mask_bounds,'raster_memory':portable_raster_memory_build.record(args),'sliced_native_frames':args.sliced_native_frames,'raster_work_budget_ms':8 if args.sliced_native_frames else 2,'last_attempt_checkpoint':args.last_attempt_checkpoint,'watch_native_ui':args.watch_native_ui,'build_defines':flags,'catalog':catalog_record,'purpose':'portable-development-artifact-not-deployment','version':version,
        'repository_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
        'full_frames':args.full_frames,'retained_rgb565_handoff':args.retained_rgb565_handoff,'touch_rotation':args.rotation,'clock_policy':'rtc-utc8-america-denver' if args.denver else 'rtc-wall-time' if args.wall_time else 'unavailable',
        'display_rotation':args.display_rotation,'navigation':args.navigation,'quick_actions':args.quick_actions,'quick_usb_transfer':args.quick_usb_transfer,'quick_radios':args.quick_radios,'home_app':args.home_app,'working_tree_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True).strip()),
        'compiler':subprocess.check_output([cc,'--version'],text=True).splitlines()[0],
        'sha256':hashlib.sha256(data).hexdigest(),'size_bytes':len(data),
        'imports':sorted(imports),'exports':sorted(exports),
        'source_sha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in inputs}}
    if args.touch_scrolling:record['touch_scrolling']={'grid':'horizontal-pages-3-by-4','apps_per_page':12,'horizontal_swipe':'finger-tracked-clamped-slide-then-snap','presentation':'latest-state-on-ready-no-frame-queue','hit_identity':'completed-frame-filename','catalog_bound':catalog_bound,'catalog_input_limit':40,'time_format':'persisted-12-or-24-hour-unpadded-hour-padded-minute'}
    if paper_transition:record['paper_transition']=paper_transition
    if args.paper_transitions:record['paper_motion']=portable_paper_build.motion_receipt(ROOT)
    if paper_transition or args.paper_transitions:record['build_defines']=flags
    portable_native_toolbar_build.record(args,record,manifest,native_receipt)
    if native_receipt:
        record['build_defines']=flags
        for p in ['scripts/build_portable_springboard.py','scripts/portable_native_toolbar_build.py','scripts/portable_alarm_build.py',native_receipt['profile_source']]:
            record['source_sha256'][p]=hashlib.sha256((ROOT/p).read_bytes()).hexdigest()
        if portable_performance_build.selected(args):
            path='scripts/portable_performance_build.py'
            record['source_sha256'][path]=hashlib.sha256((ROOT/path).read_bytes()).hexdigest()
    if args.installed_catalog_volume_instance is not None:
        record['installed_catalog']={'capability':'software.installed-apps','api':1,'volume_instance':args.installed_catalog_volume_instance,
          'storage_sdk_sha256':hashlib.sha256((includes/'RiscStorageVolumeV1.h').read_bytes()).hexdigest(),
          'discovery':'copied-async-owner-polls','idle_settings_maintenance':'settings_pending-v1','settings_retry_ms':2000,'shared_preferences_retry':'foreground-250ms','cache':'internal-app-data','status':'experimental-integration'}
    if args.installed_catalog_volume_instance is not None:
        for path in ['lib/PortableApps/src/PortableInstalledCatalog.c','lib/PortableApps/include/PortableInstalledCatalog.h','lib/PortableApps/include/InstalledAppsV1.h','lib/PortableApps/src/installed_catalog_adapter.inc']:
            record['source_sha256'][path]=hashlib.sha256((ROOT/path).read_bytes()).hexdigest()
    portable_broadcast_build.record(args,ROOT,record,manifest,flags)
    portable_idle_build.record(args,record,flags)
    portable_quick_build.record(args,record)
    portable_quick_build.record(args,record)
    portable_foreground_idle_build.record(args,record,flags,compile_command=compile_command)
    portable_native_toolbar_build.write_admission(ROOT,out,manifest,record)
    portable_native_performance_build.record(ROOT,native_perf_receipt,record)
    if args.springboard_gesture_diagnostics:
        record['springboard_gesture_diagnostics']={'schema':1,'classification':'bounded-terminal-gesture-diagnostic-not-performance-metric','selector':'PORTABLE_SPRINGBOARD_GESTURE_TRACE','tags':[224,225,226,227,228],'max_records_per_gesture':14,'max_semantic_points':4,'provider_coordinates':'decoded-not-raw-CST-bytes','scope':'Springboard-owned only; suppress Quick/alarm/other apps','transport':'existing-owner-RAM-trace; explicit perf capture only','physical_verified':False}
    (out/'springboard-build-record.json').write_text(json.dumps(record,indent=2)+'\n')
    print('Portable Springboard: target layout, ELF validator, import/export checks passed')

if __name__ == '__main__': build()
