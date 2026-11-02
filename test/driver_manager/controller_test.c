#include "../../Apps/driver_manager_controller.h"
#include <assert.h>
int main(void){
 dm_controller c;dm_controller_init(&c);
 driver_manager_snapshot_v1 s={0};driver_manager_row_v1 r={0};driver_manager_selection_v1 k;uint32_t op;
 s.generation=1;s.count=1;s.available_sources=7;s.source=DRIVER_MANAGER_INBOX;
 r.token=7;r.flags=DRIVER_MANAGER_VALID|DRIVER_MANAGER_CAN_INSTALL|DRIVER_MANAGER_CAN_UNINSTALL;
 strcpy(r.id,"sensor");strcpy(r.version,"2");strcpy(r.installed_version,"1");
 assert(dm_controller_copy(&c,&s,&r));assert(dm_controller_select(&c,0));
 assert(dm_controller_prepare(&c,DM_UNINSTALL));
 driver_manager_snapshot_v1 newer=s;++newer.generation;
 assert(!dm_controller_confirm(&c,&newer,&r,&op,&k));
 driver_manager_row_v1 replaced=r;++replaced.token;
 assert(!dm_controller_confirm(&c,&s,&replaced,&op,&k));
 assert(dm_controller_confirm(&c,&s,&r,&op,&k));assert(op==DRIVER_MANAGER_UNINSTALL&&k.token==7);
 assert(!dm_controller_confirm(&c,&s,&r,&op,&k));
 assert(dm_controller_select(&c,0));assert(dm_controller_prepare(&c,DM_INSTALL));
 assert(dm_controller_copy(&c,&newer,&r));assert(!c.selected&&!c.view.confirming);
 s.result=DRIVER_MANAGER_WORKING;s.custody=DRIVER_MANAGER_CUSTODY_PENDING;
 assert(dm_controller_copy(&c,&s,&r));assert(c.view.busy&&!dm_controller_clean(&c));assert(!dm_controller_select(&c,0));
 s.result=DRIVER_MANAGER_RETAINED;s.custody=0;
 assert(dm_controller_copy(&c,&s,&r));assert(c.terminal&&!dm_controller_clean(&c));
 dm_controller_init(&c);s.result=0;s.source=DRIVER_MANAGER_RECOVERY;r.id[0]=0;r.flags=DRIVER_MANAGER_CAN_DISCARD;
 assert(dm_controller_copy(&c,&s,&r));assert(dm_controller_select(&c,0));assert(dm_controller_prepare(&c,DM_DISCARD));
 assert(dm_controller_confirm(&c,&s,&r,&op,&k));assert(op==DRIVER_MANAGER_DISCARD&&!k.id[0]);
 r.id[0]='x';memset(r.id,'x',sizeof(r.id));assert(!dm_controller_copy(&c,&s,&r));
 return 0;
}
