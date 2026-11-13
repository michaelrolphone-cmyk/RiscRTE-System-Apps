"""Explicit, independently recorded snapshot memory-traffic selections."""
def add_arguments(parser):
    parser.add_argument('--raster-overwrite-clear', action='store_true', help='Elide only proven complete-overwrite RGB565 clears (default off)')
    parser.add_argument('--raster-underlay-exchange', action='store_true', help='Exchange eligible completed raster/alarm buffer roles (default off)')

def configure(args, parser, effective_flags=()):
    clear=bool(getattr(args,'raster_overwrite_clear',False))
    exchange=bool(getattr(args,'raster_underlay_exchange',False))
    if (clear or exchange) and not getattr(args,'sliced_native_frames',False):
        parser.error('Raster memory selections require --sliced-native-frames')
    if exchange and '-DPORTABLE_ALARM_CLIENT' not in effective_flags:
        parser.error('--raster-underlay-exchange requires an effective alarm client selection')
    return (['-DPORTABLE_RASTER_OVERWRITE_CLEAR'] if clear else []) + (['-DPORTABLE_RASTER_UNDERLAY_EXCHANGE'] if exchange else [])

def record(args):
    return {'overwrite_clear':bool(getattr(args,'raster_overwrite_clear',False)),
            'underlay_exchange':bool(getattr(args,'raster_underlay_exchange',False)),
            'clear_scope':'RGB565 full-overwrite proof only; no saving where no clear exists',
            'exchange_scope':'ordinary unpinned exact replay only; modal/transformed/failure paths retain fallback',
            'physical_performance_verified':False}
