/* Experimental app-local immutable command snapshot. Never selected by a
 * product build until all renderer/custody equivalence gates pass. */
enum {RS_FILL,RS_ROUNDED,RS_TEXT,RS_ICON,RS_PP_TEXT,RS_PP_CIRCLE,
 RS_NP_ICON,RS_NP_CIRCLE,RS_NP_CAPTION,RS_NP_MASK};
typedef struct raster_command {
 struct raster_command *next;
 unsigned kind;
 int32_t args[8];
 bool text_present;
 char text[129];
#ifdef PORTABLE_TOUCH_SCROLL
 bool clipped;
 portable_scroll_viewport clip;
#endif
} raster_command;
static raster_command *raster_commands,*raster_tail,*raster_cursor;
static bool raster_recording,raster_sealed,raster_replaying,raster_full,raster_begin_allowed;
static bool raster_saved_rotation;
#ifdef PORTABLE_PAPER_PREFERENCES
static bool raster_saved_flip;
#endif
static int raster_band_top,raster_band_bottom;
static unsigned raster_command_count;
static bool raster_progress(void);
static bool raster_drain(void);
static bool raster_begin(uint16_t color);
static bool raster_record(unsigned kind,const int32_t args[8],const char *text);
static void present(bool full);
static bool raster_materialize(void);
