/* App-local immutable commands and separately owned software pixels. One
 * prepared frame may progress alongside one immutable provider submission. */
enum {RS_FILL,RS_ROUNDED,RS_TEXT,RS_ICON,RS_PP_TEXT,RS_PP_CIRCLE,
 RS_NP_ICON,RS_NP_CIRCLE,RS_NP_CAPTION,RS_NP_MASK,RS_NP_PIXEL,
 RS_NU_PIXEL,RS_NU_RECT,RS_NU_TEXT,RS_NU_ROUND,RS_NU_BUTTON,
 RS_SV_PIXEL,RS_SV_RECT,RS_SV_TEXT,RS_SV_BUTTON,RS_SV_FADE,
 RS_WV_PIXEL,RS_WV_RECT,RS_WV_TEXT,RS_WV_BUTTON,
 RS_SBH_TEXT,RS_QR_TEXT,RS_QR_ROUND,RS_QR_ICON,RS_QP_ICON,RS_BITMAP,RS_QA_RENDER,RS_CUSTOM};
typedef struct raster_command {
 struct raster_command *next;
 unsigned kind;
 int32_t args[8];
 void *pixels;
 void (*custom)(const void *,int,int);
 bool text_present;
 int paper_offset;
 char text[129];
#ifdef PORTABLE_TOUCH_SCROLL
 bool clipped;
 portable_scroll_viewport clip;
#endif
} raster_command;
#define RASTER_COMMANDS_PER_BLOCK 32u
typedef struct raster_command_block {
 struct raster_command_block *next;
 raster_command commands[RASTER_COMMANDS_PER_BLOCK];
} raster_command_block;
static raster_command_block *raster_blocks,*raster_block_tail;
static raster_command *raster_commands,*raster_tail,*raster_cursor;
static bool raster_recording,raster_sealed,raster_replaying,raster_full,raster_begin_allowed;
static bool raster_ready_to_submit,raster_band_columns;
static void *raster_offscreen;
static bool raster_saved_rotation;
#ifdef PORTABLE_PAPER_PREFERENCES
static bool raster_saved_flip;
#endif
static int raster_band_top,raster_band_bottom,raster_band_limit;
/* Restrict primitive loops to the logical replay band before their expensive
 * per-pixel work. Keep original coordinates for coverage, clipping, rotation
 * and blending; the existing pixel/fill guards remain the final authority.
 * Widen only subtraction/comparison, never division (no new target helpers). */
static inline void raster_rows(int64_t origin,int *first,int *end) {
 if(!raster_replaying)return;
 int64_t top=(int64_t)raster_band_top-origin;
 int64_t bottom=(int64_t)raster_band_bottom-origin;
 if(top>*first)*first=top<*end?(int)top:*end;
 if(bottom<*end)*end=bottom>*first?(int)bottom:*first;
}
static unsigned raster_command_count;
static uint32_t raster_saved_intent;
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
static uint32_t raster_handoff_origin;
#endif
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
static void raster_discard(void);
static inline bool raster_immediate(void);

static inline bool raster_terminal(void) {
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
 if(native_custody_retained)return true;
#endif
#ifdef PORTABLE_TEXT_INPUT_CLIENT
 if(text_input_retained)return true;
#endif
#ifdef PORTABLE_ALARM_CLIENT
 if(native_sleep_retained)return true;
#endif
 return false;
}
