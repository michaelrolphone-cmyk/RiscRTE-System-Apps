#!/usr/bin/env python3
"""Read committed paper font tables and inspect every nonresident Quick label.

No renderer execution, compilation, production-source edits, or hardware claim.
The default repository is the directory containing this build output directory.
Usage: python3 build/paper-label-inspection/inspect_paper_labels.py
"""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--revision', default='1d61e92e5aeeb4c2df0db6be49a38f94176eb06e',
                        help='Committed source revision; working files are never inspection inputs')
    parser.add_argument('--output', type=Path, default=Path(__file__).with_name('inspection.json'))
    args = parser.parse_args()
    root = args.root.resolve()
    source_names = [
        'lib/PortableApps/src/quick_paper.inc',
        'lib/PortableApps/src/paper.inc',
        'lib/PortableApps/src/quick_adapter.inc',
        'lib/PortableApps/src/home_settings_availability.inc',
        'lib/PortableApps/paper_fonts/text.inc',
        'lib/PortableApps/paper_fonts/SOURCES.json',
        'Apps/PaperPresentation.h',
        'scripts/generate_paper_fonts.py',
        'scripts/test_paper_coordinate_adapter.py',
        'test/native_apps/paper_coordinate_test.c',
        'test/native_apps/paper_coordinate_quick_fixture.c',
        'test/native_apps/paper_coordinate_clock_fixture.c',
    ]
    revision = subprocess.check_output(
        ['git', '-C', str(root), 'rev-parse', args.revision], text=True).strip()
    sources = {name: subprocess.check_output(
        ['git', '-C', str(root), 'show', revision + ':' + name]) for name in source_names}
    text = sources['lib/PortableApps/paper_fonts/text.inc'].decode()
    rows = [tuple(map(int, row[:5])) + (row[5],) for row in re.findall(
        r'\{(\d+),(\d+),(-?\d+),(-?\d+),(\d+),(rpp_\d+_\d+)\}', text)]
    bitmaps = {name: bytes(map(int, data.split(','))) for name, data in re.findall(
        r'static const uint8_t (rpp_\d+_\d+)\[\] = \{([^}]+)\}', text)}
    assert len(rows) == 190 and len(bitmaps) == 190
    pins = json.loads(sources['lib/PortableApps/paper_fonts/SOURCES.json'])
    assert hashlib.sha256(sources['lib/PortableApps/paper_fonts/text.inc']).hexdigest() == pins['text.inc_sha256']

    # Same 1,566-entry inspection set as the original read-only analysis.
    # These are distinct state/slot cases, not a count of unique string values.
    labels = [
        ('TAP TO TURN OFF', 1, 360), ('QUICK ACTIONS', 1, 300),
        ('FRONTLIGHT', 0, 400), ('BACKLIGHT', 0, 280),
        ('NOTIFICATIONS', 0, 400), ('SOUND UNAVAILABLE', 0, 400),
        *[(value, 0, 126) for value in [
            'SILENT', 'DND', 'AIRPLANE', 'WI-FI', 'BLUETOOTH', 'TORCH', 'UNAVAILABLE']],
        *[(value, 0, 90) for value in ['ON', 'OFF', '--']],
        *[(value, 0, 360) for value in ['USB SD TRANSFER', 'SWIPE UP TO CLOSE']],
        ('CHANGE UNCONFIRMED', 0, 400), ('BATT', 0, 70),
    ]
    notice_source = sources['lib/PortableApps/src/home_settings_availability.inc'].decode()
    labels += [(value, 0, 400) for value in re.findall(r'"([A-Z ;]+)"', notice_source)]
    labels += [(value, 0, 70) for value in ['--%'] + [f'{value}%' for value in range(101)]]
    labels += [(f'{hour:02}:{minute:02}', 1, 112)
               for hour in range(24) for minute in range(60)] + [('--:--', 1, 112)]
    assert len(labels) == 1566

    glyphs = {}
    for width, height, left, top, advance_q4, name in rows:
        points = []
        for y in range(height):
            for x in range(width):
                bit = y * width + x
                if bitmaps[name][bit // 8] & (128 >> (bit % 8)):
                    points.append((left + x, top + y))
        bounds = ([min(p[0] for p in points), min(p[1] for p in points),
                   max(p[0] for p in points) + 1, max(p[1] for p in points) + 1]
                  if points else None)
        glyphs[name] = {'width': width, 'height': height, 'left': left, 'top': top,
                        'advance_q4': advance_q4, 'ink_bounds_half_open': bounds}

    entries = []
    for index, (label, heading, slot) in enumerate(labels):
        pen = 0
        bounds = None
        for character in label:
            glyph = rows[(0 if heading else 95) + ord(character) - 32]
            ink = glyphs[glyph[5]]['ink_bounds_half_open']
            if ink is not None:
                current = [pen // 16 + ink[0], ink[1], pen // 16 + ink[2], ink[3]]
                bounds = current if bounds is None else [
                    min(bounds[0], current[0]), min(bounds[1], current[1]),
                    max(bounds[2], current[2]), max(bounds[3], current[3])]
            pen += glyph[4]
        measured = (pen + 15) // 16
        minimum_width = (measured * 480 + slot - 1) // slot
        entries.append({
            'index': index, 'text': label, 'heading': bool(heading),
            'font': 'Orbitron-700-28px' if heading else 'Rajdhani-600-20px',
            'reference_slot_width': slot, 'advance_q4': pen,
            'measured_width': measured, 'ink_bounds_half_open': bounds,
            'slot_widths': {str(width): slot * width // 480 for width in [400, 410, 480, 600]},
            'minimum_display_width_for_unfitted_advance': minimum_width,
            'clips_supported_widths': ([400, min(600, minimum_width - 1)]
                                       if minimum_width > 400 else None),
            'overflows_480': measured > slot,
            'ink_overhang': bounds is not None and (bounds[0] < 0 or bounds[2] > measured),
        })

    def entry(label):
        return next(value for value in entries if value['text'] == label)

    assert not any(value['overflows_480'] for value in entries)
    assert not any(value['ink_overhang'] for value in entries)
    clock_entries = [value for value in entries if re.fullmatch(r'\d{2}:\d{2}', value['text'])]
    widest_clock = max(value['measured_width'] for value in clock_entries)
    assert len(clock_entries) == 1440 and widest_clock == 101
    assert entry('20:59')['minimum_display_width_for_unfitted_advance'] == 429
    assert entry('UNAVAILABLE')['minimum_display_width_for_unfitted_advance'] == 420
    assert entry('QUICK ACTIONS')['minimum_display_width_for_unfitted_advance'] == 404
    assert entry('00:00')['minimum_display_width_for_unfitted_advance'] == 433
    minimum_slack = min(value['reference_slot_width'] - value['measured_width'] for value in entries)
    assert minimum_slack == 11

    result = {
        'method': 'Static Q4-advance and actual packed-bitmap inspection; no runtime fixture executed',
        'source_head': revision,
        'source_tree': subprocess.check_output(['git', '-C', str(root), 'rev-parse', revision + '^{tree}'], text=True).strip(),
        'source_read_mode': 'git show from the pinned revision, never concurrently edited working files',
        'source_sha256': {name: hashlib.sha256(sources[name]).hexdigest() for name in source_names},
        'script_sha256': sha256(Path(__file__)),
        'font_provenance': pins,
        'capability_eligibility': {'width_inclusive': [400, 600], 'minimum_height': 600,
                                   'height_must_exceed_width': True,
                                   'retains_image': True, 'MONO1_supported': True,
                                   'source': 'lib/PortableApps/src/paper.inc:104-112'},
        'count': len(entries), 'clock_count': len(clock_entries),
        'overflow_480_count': sum(value['overflows_480'] for value in entries),
        'ink_overhang_count': sum(value['ink_overhang'] for value in entries),
        'minimum_480_advance_slack': minimum_slack,
        'thresholds': {label: {key: entry(label)[key] for key in [
            'advance_q4', 'measured_width', 'reference_slot_width', 'slot_widths',
            'minimum_display_width_for_unfitted_advance', 'clips_supported_widths']}
            for label in ['QUICK ACTIONS', 'UNAVAILABLE', '20:59', '00:00']},
        'widest_clocks': [value['text'] for value in clock_entries if value['measured_width'] == widest_clock],
        'source_evidence': {
            'scaled_slots': 'lib/PortableApps/src/quick_paper.inc:30-31,93,123-124',
            'advance_measurement': 'lib/PortableApps/src/paper.inc:21-32',
            'whole_glyph_rejection_and_fit_gating': 'lib/PortableApps/src/paper.inc:38-85',
            'full_fixture_time': 'test/native_apps/paper_coordinate_clock_fixture.c:194',
            'time_string_construction': 'lib/PortableApps/src/quick_adapter.inc:345-380',
            'fixed_font_sizes': 'scripts/generate_paper_fonts.py:9-18',
            'notice_strings': 'lib/PortableApps/src/home_settings_availability.inc:20-24',
        },
        'glyphs': glyphs,
        'entries': entries,
        'limitations': [
            'No production-source edits or compilations',
            'No new production fixture execution or hardware verification',
            'Conditional FIT preservation at 480 is inferred from all measured labels fitting their original slots',
            'Font and source hashes pin this inspection; changed assets or labels require rerunning and reviewing the set',
        ],
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'receipt': str(args.output.resolve()), 'sha256': sha256(args.output),
                      'script_sha256': result['script_sha256'], 'entries': len(entries),
                      'overflow_480_count': result['overflow_480_count'],
                      'ink_overhang_count': result['ink_overhang_count'],
                      'thresholds': {label: value['minimum_display_width_for_unfitted_advance']
                                     for label, value in result['thresholds'].items()}}, indent=2))


if __name__ == '__main__':
    main()
