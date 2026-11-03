#include "DriverManagerSelection.h"
#include <assert.h>
int main(void){
 driver_manager_snapshot_v1 s={0};driver_manager_row_v1 r={0};driver_manager_selection_v1 k;
 s.generation=1;s.count=1;s.available_sources=7;r.token=1;r.flags=DRIVER_MANAGER_VALID|DRIVER_MANAGER_CAN_INSTALL;
 strcpy(r.id,"driver");strcpy(r.version,"1.0.0");
 assert(driver_manager_select(&s,0,&r,&k));assert(driver_manager_selection_matches(&s,&k,&r));
 assert(driver_manager_action_allowed(s.source,DRIVER_MANAGER_INSTALL,&r,false));
 ++s.generation;assert(!driver_manager_selection_matches(&s,&k,&r));--s.generation;
 ++r.token;assert(!driver_manager_selection_matches(&s,&k,&r));--r.token;
 strcpy(r.version,"2.0.0");assert(!driver_manager_selection_matches(&s,&k,&r));strcpy(r.version,"1.0.0");
 s.custody=DRIVER_MANAGER_CUSTODY_PENDING;assert(!driver_manager_select(&s,0,&r,&k));s.custody=0;
 s.result=DRIVER_MANAGER_WORKING;assert(!driver_manager_select(&s,0,&r,&k));s.result=0;
 s.result=DRIVER_MANAGER_RETAINED;assert(!driver_manager_select(&s,0,&r,&k));s.result=0;
 s.result=DRIVER_MANAGER_NEEDS_NETWORK;assert(!driver_manager_select(&s,0,&r,&k));assert(!driver_manager_selection_matches(&s,&k,&r));s.result=0;
 s.result=123;assert(!driver_manager_snapshot_valid(&s));s.result=0;
 s.available_sources=0;assert(!driver_manager_select(&s,0,&r,&k));s.available_sources=7;
 assert(!driver_manager_select(&s,1,&r,&k));
 r.flags|=DRIVER_MANAGER_CAN_UNINSTALL;strcpy(r.installed_version,"0.9.0");
 assert(!driver_manager_action_allowed(0,DRIVER_MANAGER_UNINSTALL,&r,false));
 assert(driver_manager_action_allowed(0,DRIVER_MANAGER_UNINSTALL,&r,true));
 s.source=DRIVER_MANAGER_RECOVERY;r.id[0]=0;r.version[0]=0;r.flags=DRIVER_MANAGER_CAN_RETRY|DRIVER_MANAGER_CAN_DISCARD;
 assert(driver_manager_select(&s,0,&r,&k));assert(driver_manager_selection_matches(&s,&k,&r));
 assert(driver_manager_action_allowed(s.source,DRIVER_MANAGER_RETRY,&r,false));
 assert(!driver_manager_action_allowed(s.source,DRIVER_MANAGER_DISCARD,&r,false));
 assert(driver_manager_action_allowed(s.source,DRIVER_MANAGER_DISCARD,&r,true));
 assert(!driver_manager_action_allowed(0,DRIVER_MANAGER_DISCARD,&r,true));
 assert(!driver_manager_action_allowed(s.source,DRIVER_MANAGER_INSTALL,&r,true));
 memset(r.id,'x',sizeof(r.id));assert(!driver_manager_row_valid(&r));r.id[0]=0;
 r.flags|=128;assert(!driver_manager_row_valid(&r));r.flags=0;
 s.generation=0;assert(!driver_manager_snapshot_valid(&s));s.generation=1;
 s.count=66;assert(!driver_manager_snapshot_valid(&s));
 return 0;
}
