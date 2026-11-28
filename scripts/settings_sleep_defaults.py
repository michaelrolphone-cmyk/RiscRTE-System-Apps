"""Generic opt-in Settings fallback/display policy, without hardware grants."""
import argparse


def idle_seconds(value):
    try:
        seconds=int(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError('idle seconds must be an integer from 5 to 3600') from error
    if not 5 <= seconds <= 3600:
        raise argparse.ArgumentTypeError('idle seconds must be from 5 to 3600')
    return seconds


def options(parser):
    parser.add_argument('--default-sleep-mode',choices=['deep','light','hybrid'],
                        help='Sleep choice fallback; valid saved modes win; requires --sleep-settings')
    parser.add_argument('--default-idle-seconds',type=idle_seconds,
                        help='Idle fallback, 5–3600 seconds; valid saved values win; requires --idle-timer-settings or --sleep-timer-settings')
    parser.add_argument('--idle-timer-settings',action='store_true',
                        help='Show one editable idle timer without adding sleep/battery/radio providers; requires --sleep-settings')

    parser.add_argument('--sleep-timer-settings',action='store_true',
                        help='Show independent idle and subsequent Deep timers; requires --sleep-settings')


def configure(args,parser):
    mode=getattr(args,'default_sleep_mode',None)
    idle=getattr(args,'default_idle_seconds',None)
    timer=getattr(args,'idle_timer_settings',False)
    both=getattr(args,'sleep_timer_settings',False)
    if timer and both:parser.error('Select either --idle-timer-settings or --sleep-timer-settings')
    if mode is None and idle is None and not timer and not both:return []
    if getattr(args,'settings_profile','default') not in ('default','native-time-touch'):
        parser.error('Generic sleep defaults and idle timer settings require default or native-time-touch Settings')
    if (mode is not None or timer or both) and not args.sleep_settings:
        parser.error('Sleep defaults and idle timer settings require --sleep-settings')
    if idle is not None and not (timer or both):
        parser.error('--default-idle-seconds requires --idle-timer-settings or --sleep-timer-settings')
    flags=[]
    if mode is not None:flags.append('-DPORTABLE_SETTINGS_DEFAULT_SLEEP_MODE=PORTABLE_SLEEP_'+mode.upper())
    if timer:flags.append('-DPORTABLE_SETTINGS_IDLE_TIMER')
    if both:flags.append('-DPORTABLE_SETTINGS_BOTH_TIMERS')
    if idle is not None:flags.append(f'-DPORTABLE_SETTINGS_DEFAULT_IDLE_SECONDS={idle}u')
    return flags


def record(args):
    return {'mode':getattr(args,'default_sleep_mode',None) or 'hybrid',
            'idle_seconds':getattr(args,'default_idle_seconds',None) if getattr(args,'default_idle_seconds',None) is not None else 60,
            'scope':'settings-fallback-only','persisted_preferences_take_precedence':True,
            'idle_timer_editable':bool(getattr(args,'idle_timer_settings',False) or getattr(args,'sleep_timer_settings',False)),
            'deep_timer_editable':bool(getattr(args,'sleep_timer_settings',False)),
            'sleep_backend':False}
