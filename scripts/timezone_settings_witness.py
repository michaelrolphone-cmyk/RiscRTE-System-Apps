"""Current-source isolation checks for the optional Settings timezone editor.

The small eraser deliberately supports only the explicit #ifdef gates used by
the two Settings integration files. It is not a general C preprocessor. New
gate syntax fails closed and must be reviewed rather than silently rewritten.
"""
import re

FEATURE = 'PORTABLE_SETTINGS_TIME_ZONE'
POISON = 'TIMEZONE_SETTINGS_WITNESS_POISON'
INTEGRATION = ('lib/PortableApps/src/settings.inc',
               'lib/PortableApps/src/settings_view.inc')
FEATURE_FILES = tuple('lib/PortableApps/' + path for path in (
    'src/settings_timezone.inc', 'src/settings_timezone_view.inc',
    'src/PortableTimeZone.c', 'src/PortableTimeZoneCatalog.c',
    'src/PortableTimeZonePreference.c', 'include/PortableTimeZone.h',
    'include/PortableTimeZonePreference.h'))
TIMEZONE_IDENTIFIER = re.compile(r'\b(?:stz_\w*|portable_timezone_\w*|SV_TIMEZONE_\w*)\b')


def erase_timezone(source):
    """Remove positive timezone branches; retain their ordinary #else paths."""
    output, stack, gates = [], [], 0
    for number, line in enumerate(source.splitlines(keepends=True), 1):
        directive = re.match(r'^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)', line)
        visible = all(frame['visible'] for frame in stack)
        if not directive:
            if visible:
                output.append(line)
            continue
        kind, argument = directive.groups()
        argument = re.sub(r'/\*.*?\*/|//.*', '', argument).strip()
        if kind in ('if', 'ifdef', 'ifndef'):
            feature = bool(re.search(r'\b' + FEATURE + r'\b', argument))
            if feature and (kind != 'ifdef' or argument != FEATURE):
                raise ValueError(f'Unsupported timezone gate at line {number}')
            if visible and not feature:
                output.append(line)
            stack.append({'feature': feature, 'visible': not feature, 'else': False})
            gates += int(feature)
        elif kind in ('else', 'elif'):
            if kind == 'elif' and re.search(r'\b' + FEATURE + r'\b', argument):
                raise ValueError(f'Unsupported timezone elif at line {number}')
            if not stack or stack[-1]['else']:
                raise ValueError(f'Unmatched or repeated branch at line {number}')
            frame = stack[-1]
            if frame['feature']:
                if kind == 'elif':
                    raise ValueError(f'Unsupported timezone elif at line {number}')
                frame['visible'] = True
            elif all(parent['visible'] for parent in stack[:-1]):
                output.append(line)
            frame['else'] = kind == 'else'
        else:
            if not stack:
                raise ValueError(f'Unmatched endif at line {number}')
            frame = stack.pop()
            if not frame['feature'] and all(parent['visible'] for parent in stack):
                output.append(line)
    if stack:
        raise ValueError('Unclosed preprocessor gate')
    if not gates:
        raise ValueError('No explicit timezone gates found')
    return ''.join(output), gates


def assert_timezone_absent(text, description):
    leaked = sorted(set(TIMEZONE_IDENTIFIER.findall(text)))
    if leaked:
        raise AssertionError(f'{description}: timezone identifiers leaked: {leaked}')


def assert_identical(before, after, description):
    if before != after:
        raise AssertionError(f'{description}: current feature-off projection changed')


def prepare_projection(root, poison_only=False):
    """Mutate only the caller-owned temporary source copy."""
    counts = {}
    if not poison_only:
        for name in INTEGRATION:
            path = root / name
            erased, count = erase_timezone(path.read_text())
            path.write_text(erased)
            counts[name] = count
    for name in FEATURE_FILES:
        path = root / name
        if not path.is_file():
            raise ValueError(f'Missing timezone isolation input: {name}')
        path.write_text(f'#error {POISON}\n')
    return counts
