#ifndef PORTABLE_SETTINGS_MODEL_H
#define PORTABLE_SETTINGS_MODEL_H
/* One semantic Settings controller. Namespace-1 tables are borrowed for the
 * invocation; this object is never copied or persisted after initialization.
 * No display, input, product identity or backend selection enters this API. */
#include "PortableSetTime.h"
#include "PortablePreferenceAtomic.h"
#include "PortableTimeZonePreference.h"

typedef enum {
 PSM_TIME=0, PSM_ZONE, PSM_FORMAT, PSM_BASIS, PSM_SLEEP, PSM_ALERT,
 PSM_FACE, PSM_FLIP, PSM_LANGUAGE, PSM_DIRECTION, PSM_IDLE_TIMER,
 PSM_DEEP_TIMER, PSM_STORAGE, PSM_ABOUT, PSM_FEATURE_COUNT
} psm_feature;
#define PSM_FEATURE(f) (UINT32_C(1) << (f))
#define PSM_ALL_FEATURES ((UINT32_C(1) << PSM_FEATURE_COUNT)-1u)
enum { PSM_ROOT=1, PSM_EDITOR=2, PSM_FOLD=3, PSM_STORAGE_ROUTE=4, PSM_ABOUT_ROUTE=5 };
enum { PSM_ACTION_OPEN=1, PSM_ACTION_SET=2, PSM_ACTION_SAVE=3,
 PSM_ACTION_CANCEL=4, PSM_ACTION_DISCARD=5, PSM_ACTION_RELOAD=6,
 PSM_ACTION_FOLD=7 };
enum { PSM_LOADED=0, PSM_MISSING=1, PSM_INVALID=2, PSM_UNAVAILABLE=3,
 PSM_UNSUPPORTED=4, PSM_UNCONFIRMED=5, PSM_CONFLICT=6 };
enum { PSM_IDLE=0, PSM_WAIT_PRESENTATION, PSM_COMMITTING, PSM_CANCELLING, PSM_RETAINED };
/* Callback outcomes. PENDING means custody remains with the operation and only
 * poll/cancel is legal. RETAINED permanently freezes this model's service I/O. */
enum { PSM_OK=0, PSM_PENDING=1, PSM_CANCELLED=2, PSM_ERROR=-1,
 PSM_BUSY=-2, PSM_OPERATION_RETAINED=-9 };
enum { PSM_STORAGE_AVAILABLE=0, PSM_STORAGE_UNAVAILABLE=1,
 PSM_STORAGE_RELOAD_REQUIRED=2, PSM_STORAGE_CONFLICT=3 };
typedef struct {
 uint32_t number; /* timers use milliseconds; other choices use persisted IDs */
 char zone[PORTABLE_TIMEZONE_ID_BYTES];
 portable_timezone_civil civil;
 portable_rtc_basis basis;
} psm_value;
typedef struct {
 uint32_t features,writable;
 unsigned sleep_modes,sleep_default,face_default;
 uint32_t idle_default_ms,deep_default_ms;
 bool desk_lock,visual_alerts;
} psm_policy;
typedef struct {
 void *context;
 bool (*live)(void *); /* REQUIRED, checked before and after provider calls */
 /* Clock callbacks must preserve alarm clock-edit authority, clock/metadata
  * ordering, preflight, exact verification and independent partial outcomes.
  * read fills civil; begin captures editor authority before draft exposure.
  * begin/read are bounded synchronous calls and must not return PENDING.
  * start copies request before return; poll/cancel service bounded work only.
  * Existing PortableSetTime preparation is performed by this model. */
 int (*clock_begin)(void *);
 int (*clock_read)(void *,portable_timezone_civil *);
 int (*clock_start)(void *,const portable_set_time_request *,portable_set_time_result *);
 int (*clock_poll)(void *,portable_set_time_result *);
 int (*clock_cancel)(void *,portable_set_time_result *);
 /* Optional availability callback: no recovery/write implied by observing it. */
 int (*storage_state)(void *,unsigned *state);
 /* Called after a confirmed preference save, for live configuration apply.
  * Failure leaves persisted state confirmed but reports apply unconfirmed.
  * This callback cannot return PENDING or retain an asynchronous operation. */
 int (*applied)(void *,psm_feature,const psm_value *);
} psm_services;
typedef struct {
 psm_policy policy;psm_services services;
 const risc_key_value_v1 *store;const risc_key_value_atomic_v1 *atomic_store;
 portable_preference_atomic atomic;
 psm_value confirmed[PSM_FEATURE_COUNT],draft;
 int status[PSM_FEATURE_COUNT];bool has_confirmed[PSM_FEATURE_COUNT];
 uint32_t route,revision,save_revision;psm_feature editing;
 unsigned phase,storage_state;int last_result,fold_choice;
 bool dirty,uncertain,apply_unconfirmed,clock_active,cancel_requested;
 bool operation_started;
 bool draft_valid; /* invalid alert values require an explicit selection */
 portable_set_time_request clock_request;portable_set_time_result clock_result;
 char message[96];
} PortableSettingsModel;
psm_policy psm_default_policy(void);
bool psm_init(PortableSettingsModel *,const psm_policy *,const psm_services *,
 const risc_key_value_v1 *,const risc_key_value_atomic_v1 *);
bool psm_refresh(PortableSettingsModel *); /* root only; never overwrites draft */
bool psm_open(PortableSettingsModel *,psm_feature);
bool psm_set_number(PortableSettingsModel *,uint32_t);
bool psm_set_zone(PortableSettingsModel *,const char *,size_t);
bool psm_set_civil(PortableSettingsModel *,const portable_timezone_civil *);
bool psm_choose_fold(PortableSettingsModel *,unsigned);
bool psm_save(PortableSettingsModel *);
/* Explicit user rebase only: preserves chosen draft, captures fresh atomic
 * preconditions. No automatic retry, write or storage recovery. */
bool psm_reload_draft(PortableSettingsModel *);
/* Call only when the exact save_revision's uncertainty notice is visibly
 * settled, not merely queued. No writes happen before this acknowledgement. */
bool psm_presented(PortableSettingsModel *,uint32_t revision);
void psm_tick(PortableSettingsModel *); /* at most one operation step; no waits */
void psm_cancel(PortableSettingsModel *); /* cancels work; preserves uncertain draft */
bool psm_discard(PortableSettingsModel *); /* explicit draft abandonment only */
bool psm_can_handoff(const PortableSettingsModel *);
const char *psm_key(psm_feature);
const char *psm_label(psm_feature);
const char *psm_choice_label(psm_feature,unsigned);
unsigned psm_choice_count(psm_feature);
#endif
