#ifndef PORTABLE_SETTINGS_SCENE_H
#define PORTABLE_SETTINGS_SCENE_H
#include "PortableSettingsModel.h"
#include "RiscSceneComponentsV1.h"
/* Invocation-local semantic browsing state. No display dimensions or paging.
 * Initialize once; build after any model or browsing change. Providers own all
 * physical presentation. Never acknowledge presentation from an input event. */
typedef struct {
 PortableSettingsModel *model;
 uint32_t revision,model_revision,presented_save_revision,fingerprint;
 int zone_region,zone_initial;
 unsigned count;
 uint32_t back_action;
 bool exit_requested,reload_requested;
 struct { uint32_t id,action,kind,flags; int32_t minimum,maximum; } emitted[RISC_COMPONENTS_MAX_NODES];
} pss_scene;
void pss_init(pss_scene *,PortableSettingsModel *);
/* Copy this view into a candidate, build/publish, and commit the candidate
 * only after successful publication. Failed publication leaves this view intact. */
bool pss_needs_publish(const pss_scene *);
bool pss_build(pss_scene *,risc_components_document_v1 *);
bool pss_action(pss_scene *,const risc_scene_event_v1 *);
/* Returns model save_revision only for the exact current document while its
 * model is still awaiting visible presentation. Caller must independently
 * establish that this document has visibly settled before psm_presented(). */
uint32_t pss_save_revision(const pss_scene *,uint32_t scene_revision);
#endif
