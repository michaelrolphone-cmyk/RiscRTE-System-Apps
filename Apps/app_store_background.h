#pragma once
/* Read-only foreground broadcaster. Reuses published Battery controller;
 * deliberately exports no preference setter/transaction API. */
#include "battery_broadcast.h"
typedef struct {
 battery_broadcast broadcast;
 telemetry_broadcast_status_v1 status;
 bool status_valid,closed;
} app_store_background;
static inline int app_store_background_guard(app_store_background*s){
 if(!battery_broadcast_guard(&s->broadcast))return battery_broadcast_retain(&s->broadcast);
 /* This wrapper never submits a storage edit, so a token is not its custody. */
 if(s->broadcast.edit.token||s->broadcast.preference.api)return battery_broadcast_retain(&s->broadcast);
 return BATTERY_BROADCAST_OK;
}
static inline int app_store_background_init(app_store_background*s,const risc_runtime_api_v1*rt,bool(*alive)(void*),void(*retain)(void*),void*context){
 memset(s,0,sizeof(*s));s->status.struct_size=sizeof(s->status);
 return battery_broadcast_init(&s->broadcast,rt,alive,retain,context);
}
static inline int app_store_background_pause(app_store_background*s){
 int rc=app_store_background_guard(s);if(rc)return rc;
 s->status_valid=false;
 if(!s->broadcast.service.api)return BATTERY_BROADCAST_OK;
 return battery_broadcast_pause(&s->broadcast);
}
static inline int app_store_background_step(app_store_background*s,bool allow){
 int rc=app_store_background_guard(s);if(rc)return rc;
 if(s->closed)return BATTERY_BROADCAST_UNAVAILABLE;
 if(!allow)return app_store_background_pause(s);
 rc=battery_broadcast_step(&s->broadcast,true);if(rc){s->status_valid=false;return rc;}
 rc=battery_broadcast_read(&s->broadcast,&s->status);s->status_valid=rc==BATTERY_BROADCAST_OK;return rc;
}
/* Copied status only: rendering never opens a demand-owned service. */
static inline bool app_store_background_status(const app_store_background*s,telemetry_broadcast_status_v1*out){
 if(!s||!out||!s->status_valid||s->broadcast.retained)return false;
 *out=s->status;return true;
}
static inline int app_store_background_close(app_store_background*s){
 int rc=app_store_background_pause(s);if(rc)return rc;
 rc=battery_broadcast_release(&s->broadcast,&s->broadcast.storage);if(rc)return rc;
 s->closed=true;return BATTERY_BROADCAST_OK;
}
