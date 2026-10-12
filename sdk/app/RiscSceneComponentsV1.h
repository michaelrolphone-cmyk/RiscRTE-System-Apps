#pragma once
/* Optional copied-value component document for ui.scene@1. The original
 * scene and lifecycle tables/layouts are unchanged. Applications describe
 * content, state and intent IDs, never coordinates, fonts or device names.
 * Component documents accept TEXT, ACTION and KEYBOARD from the base kinds;
 * use the component SWITCH/STEPPER/TIME_PICKER/ROW for other control intents.
 * PROGRESS maximum=0 is indeterminate activity; value cycles 0..2.
 * Legacy documents and navigation remain available through the original API. */
#include "RiscSceneLifecycleV1.h"
#define RISC_COMPONENTS_TAG UINT32_C(0x4e4f5631)
#define RISC_COMPONENTS_MAX_NODES 96u
enum {
    RISC_COMPONENT_SECTION=8, RISC_COMPONENT_ROW, RISC_COMPONENT_CHECK_ROW,
    RISC_COMPONENT_SWITCH, RISC_COMPONENT_STEPPER, RISC_COMPONENT_SEGMENTS,
    RISC_COMPONENT_PROGRESS, RISC_COMPONENT_EMPTY, RISC_COMPONENT_HEADER_ACTION,
    RISC_COMPONENT_MARKERS, RISC_COMPONENT_TIME_PICKER, RISC_COMPONENT_CHIP,
    RISC_COMPONENT_CARD, RISC_COMPONENT_RING, RISC_COMPONENT_STRENGTH,
    RISC_COMPONENT_STAT_ROW, RISC_COMPONENT_PROGRESS_CARD, RISC_COMPONENT_MEDIA_ROW,
    RISC_COMPONENT_STATS, RISC_COMPONENT_DISMISS_ROW, RISC_COMPONENT_TEXT_PREVIEW,
    RISC_COMPONENT_INSTRUMENT, RISC_COMPONENT_BAR_GAUGE, RISC_COMPONENT_METRIC_TILE,
    RISC_COMPONENT_PLOT, RISC_COMPONENT_TAB_BAR, RISC_COMPONENT_VALUE_HERO,
    RISC_COMPONENT_STAT_TRIPLE, RISC_COMPONENT_LOG_LINE, RISC_COMPONENT_TRACE_LEGEND, RISC_COMPONENT_STATUS_TILE, RISC_COMPONENT_CODE_ROW
};
enum { RISC_SYMBOL_NONE, RISC_SYMBOL_PLUS, RISC_SYMBOL_MORE, RISC_SYMBOL_TODAY,
       RISC_SYMBOL_LIST, RISC_SYMBOL_CLOCK, RISC_SYMBOL_EDIT, RISC_SYMBOL_DELETE,
       RISC_SYMBOL_BACK, RISC_SYMBOL_CHECK, RISC_SYMBOL_PLACE, RISC_SYMBOL_EVENT,
       RISC_SYMBOL_TRIGGER, RISC_SYMBOL_LIBRARY, RISC_SYMBOL_SETTINGS,
       RISC_SYMBOL_WORKFLOW, RISC_SYMBOL_ACTIVITY, RISC_SYMBOL_BOOK,
       RISC_SYMBOL_BOOKMARK, RISC_SYMBOL_BOOKMARK_OUTLINE, RISC_SYMBOL_FONT,
       RISC_SYMBOL_SLIDERS, RISC_SYMBOL_DOCUMENT_ADD, RISC_SYMBOL_DOCUMENT_BACK,
       RISC_SYMBOL_DOCUMENT_LIST, RISC_SYMBOL_DOCUMENT_DELETE, RISC_SYMBOL_CLOSE, RISC_SYMBOL_GAUGE, RISC_SYMBOL_CHART, RISC_SYMBOL_WARNING,
       RISC_SYMBOL_INFO, RISC_SYMBOL_BLUETOOTH, RISC_SYMBOL_PAUSE, RISC_SYMBOL_PLAY,
       RISC_SYMBOL_REFRESH, RISC_SYMBOL_BELL, RISC_SYMBOL_TERMINAL, RISC_SYMBOL_UNLINK, RISC_SYMBOL_GEAR };
enum { RISC_TONE_DEFAULT, RISC_TONE_ACCENT, RISC_TONE_SUCCESS, RISC_TONE_WARNING,
       RISC_TONE_DANGER, RISC_TONE_MUTED };
#define RISC_COMPONENTS_CONFIRM 1u
#define RISC_COMPONENTS_ALERT 2u
/* A root library can retain physical Back while omitting a drawn back button. */
#define RISC_COMPONENTS_ROOT_HEADER 4u
/* Document-page controls use the reference rounded-square steppers and compact statistics. */
#define RISC_COMPONENTS_DOCUMENT 8u
/* Compact instruments and plots; sizes still belong to the presenter. */
#define RISC_COMPONENTS_TELEMETRY 16u
/* Added copied-value kinds: PROGRESS_CARD uses label/title, text/body, choices/
 * secondary status and value/maximum. MEDIA_ROW adds an outlined marker cover.
 * STATS uses label/text as two values and choices as their two pipe-separated
 * captions. DISMISS_ROW emits action for the body and secondary_action for X. */
typedef struct {
    /* CHECK_ROW: action toggles the checkbox; secondary_action opens details.
     * ROW: the whole row emits action. SWITCH/STEPPER: only controls do.
     * SEGMENTS/MARKERS: choices emit a VALUE in [minimum,maximum].
     * Choices are pipe-separated, at most six, with no empty entries.
     * Adjacent CHIP actions wrap into a group of content-sized pills.
     * The checked state is value=1; text is secondary explanatory content.
     * CHECK_ROW without secondary_action places its checkbox at the trailing
     * edge and lets the entire row toggle it. */
    uint32_t secondary_action, symbol, marker, tone;
    char badge[16], choices[72];
} risc_component_detail_v1;
typedef struct {
    uint32_t api_version,struct_size,revision,root;
    uint32_t route_count,node_count,reserved[2];
    risc_scene_route_v1 routes[RISC_SCENE_MAX_ROUTES];
    risc_scene_node_v1 nodes[RISC_COMPONENTS_MAX_NODES];
    risc_component_detail_v1 details[RISC_COMPONENTS_MAX_NODES];
    /* A changed screen_key resets scrolling. Revisions on the same screen
     * retain/clamp the viewport. A modal's cancel action is also its Back and
     * outside-tap action; initial focus is the non-destructive cancel node. */
    uint32_t screen_key,flags,cancel_action;
    char subtitle[RISC_SCENE_TEXT];
    /* Host expires toasts after four seconds; never holds an app callback.
     * A changed token starts a new toast. Paper presents plain feedback but
     * deliberately omits undo. Applications own any undo transaction. */
    uint32_t toast_token,toast_action;
    char toast[RISC_SCENE_LABEL];
} risc_components_document_v1;
typedef struct {
    risc_scene_lifecycle_api_v1 lifecycle;
    uint32_t tag,version;
    int32_t (*open)(void *,const risc_components_document_v1 *,const risc_scene_navigation_v1 *,uint64_t *);
    int32_t (*update)(void *,uint64_t,const risc_components_document_v1 *);
} risc_scene_components_api_v1;
static inline const risc_scene_components_api_v1 *risc_scene_components_get_v1(const risc_scene_api_v1 *base){
    if(!base||base->api_version!=1||base->struct_size<sizeof(risc_scene_components_api_v1))return NULL;
    const risc_scene_components_api_v1 *a=(const risc_scene_components_api_v1 *)base;
    return a->tag==RISC_COMPONENTS_TAG&&a->version==1&&a->open&&a->update?a:NULL;
}
