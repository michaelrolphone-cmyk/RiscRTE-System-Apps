/* Experimental app-local immutable command snapshot. Never selected by a
 * product build until all renderer/custody equivalence gates pass. */
enum {RS_FILL,RS_ROUNDED,RS_TEXT,RS_ICON,RS_PP_TEXT,RS_PP_CIRCLE,
 RS_NP_ICON,RS_NP_CIRCLE,RS_NP_CAPTION,RS_NP_MASK,RS_NP_PIXEL,
 RS_NU_PIXEL,RS_NU_RECT,RS_NU_TEXT,RS_NU_ROUND,RS_NU_BUTTON,
 RS_SV_PIXEL,RS_SV_RECT,RS_SV_TEXT,RS_SV_BUTTON,RS_SV_FADE,
 RS_WV_PIXEL,RS_WV_RECT,RS_WV_TEXT,RS_WV_BUTTON,
 RS_QR_TEXT,RS_QR_ROUND,RS_QR_ICON,RS_QP_ICON,RS_BITMAP};
typedef struct raster_command {
 struct raster_command *next;
 unsigned kind;
 int32_t args[8];
 void *pixels;
 bool text_present;
 int paper_offset;
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
static uint32_t raster_saved_intent;
static bool raster_saved_app_frame;
#if defined(PORTABLE_QUICK_ACTIONS)&&defined(PORTABLE_RESIDENT_SHELL_HOST)
static bool raster_saved_clean;
#endif
static bool raster_progress(void);
static bool raster_drain(void);
static bool raster_begin(uint16_t color);
static inline bool raster_begin_pixels(const void *pixels,unsigned stride);
static bool raster_record(unsigned kind,const int32_t args[8],const char *text);
static void present(bool full);
static bool raster_materialize(void);
static inline bool raster_immediate(void);
